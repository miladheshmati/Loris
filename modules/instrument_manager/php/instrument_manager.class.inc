<?php declare(strict_types=1);
namespace LORIS\instrument_manager;
use \Psr\Http\Message\ServerRequestInterface;
use \Psr\Http\Message\ResponseInterface;

/**
 * Contains functions used to view and manage instruments installed in LORIS.
 * LINST instruments can be uploaded and installed automatically.
 *
 * @license http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 */
class Instrument_Manager extends \NDB_Menu_Filter
{
    private \NDB_Factory $_factory;

    /**
     * The LORIS base path
     *
     * @var string
     */
    private string $_path;

    const PERMISSIONS = [
        'instrument_manager_read',
        'instrument_manager_write'
    ];
    // Error messages
    const ALREADY_INSTALLED     = 'This instrument already exists in the ' .
                                  'test battery.';
    const CANNOT_WRITE_FILES    = 'Automatic installation of instruments is ' .
                                  'not possible given the current server ' .
                                  'configuration. Please contact your ' .
                                  'administrator if you require this ' .
                                  'functionality.';
    const FILE_ALREADY_EXISTS   = 'This file has already been uploaded. ' .
                                  'Please contact your administrator to ' .
                                  'install the instrument.';
    const BAD_REQUEST           = 'Uploaded file is not of type LINST or is ' .
                                  'too large.';
    const UPLOAD_INSTALL_FAILED = 'File upload is successful but the ' .
                                  'installation failed. Please contact your ' .
                                  'administrator.';
    const UPLOAD_NO_INSTALL     = 'File upload is successful but the ' .
                                  'instrument needs to be installed manually.';

    /**
     * Does the setup required for this page. By default, sets up elements
     * that are common to every type of page. May be overridden by a specific
     * page or specific page type.
     *
     * @param \LORIS\LorisInstance $loris     The LORIS Instance that is serving
     *                                        the page
     * @param Module               $module    The test name being accessed
     * @param string               $page      The subtest being accessed (may be
     *                                        an empty string)
     * @param string               $id        The identifier for the data to load
     *                                        on this page
     * @param string               $commentID The CommentID to load the data for
     * @param string               $formname  The name to give this form
     */
    public function __construct(
        \LORIS\LorisInstance $loris,
        $module,
        $page,
        $id,
        $commentID,
        $formname
    ) {
        $this->AjaxModule   = true;
        $this->skipTemplate = true;
        $this->_factory     = \NDB_Factory::singleton();
        $this->_path        = $this->_factory->config()->getSetting("base");

        parent::__construct($loris, $module, $page, $id, $commentID);
    }

    /**
     * Checking permissions
     *
     * @param \User $user The user whose access is being checked
     *
     * @return bool
     */
    function _hasAccess(\User $user) : bool
    {
        return $user->hasAnyPermission(self::PERMISSIONS);
    }

    /**
     * Handle a PSR7 Request for that endpoint.
     *
     * @param ServerRequestInterface $request The PSR15 Request being handled
     *
     * @return ResponseInterface The PSR15 response for the page.
     */
    public function handle(ServerRequestInterface $request): ResponseInterface
    {
        $method = $request->getMethod();
        if ($method == 'POST') {
            return $this->handlePOST($request);
        }

        return parent::handle($request);
    }

    /**
     * Handle a POST request for instrument upload. It moves the uploaded file
     * in the project/instruments directory then call the php script from the
     * tools directory to generate sql statement required for the installation
     * of the instrument. Finaly, it source the sql file to complete the
     * installation.
     *
     * @param ServerRequestInterface $request The PSR15 Request being handled
     *
     * @return ResponseInterface The PSR15 response for the page.
     */
    protected function handlePOST(ServerRequestInterface $request): ResponseInterface
    {
        $loris = $request->getAttribute('loris');
        // Ensure the user is allowed to upload.
        if (! $request->getAttribute('user')->hasPermission(
            'instrument_manager_write'
        )
        ) {
            return new \LORIS\Http\Response\JSON\Forbidden();
        }
        // Ensure the server is properly configured.
        if (!$this->canWriteFiles()) {
            return new \LORIS\Http\Response\JSON\InternalServerError(
                self::CANNOT_WRITE_FILES
            );
        }

        $uploaded_file = $request->getUploadedFiles()['install_file'];
        $filename      = $uploaded_file->getClientFilename();
        $instrument    = pathinfo($filename)['filename'];
        $targetdir     = new \SplFileInfo($this->_path . 'project/instruments/');
        $fullpath      = $targetdir->getPathname() . '/' . $filename;

        if ($this->instrumentExists($instrument)) {
            // An instrument with this name already exists in the test_names
            // table. Abort upload.
            return new \LORIS\Http\Response\JSON\Conflict(self::ALREADY_INSTALLED);
        }

        /* Although only .linst files should be uploaded to this module, and
         * they are just special text files, they are marked as
         * application/octet-stream upon upload.
         */
        $uploader = (new \LORIS\FilesUploadHandler($targetdir))
            ->withPermittedMIMETypes(
                'application/octet-stream'
            );

        $response = $uploader->handle($request);

        if (!in_array($response->getStatusCode(), [200, 201], true)) {
            if ($response->getStatusCode() == 409) {
                /* Don't update the file if it already exists on the back-end.
                 * Instead, inform users that an administrator must install it
                 * on their behalf.
                 * This should only happen for users on a system where automatic
                 * installation is disabled (ie. has no adminUser).
                 */
                return new \LORIS\Http\Response\JSON\Conflict(
                    self::FILE_ALREADY_EXISTS
                );
            }
            return $response;
        }

        // Scripts in tools/ often make relative imports, so we must change
        // our effective directory in order to use them.
        chdir($this->_path . "/tools");
        // Use tools/ script to generate an SQL patch file based on the
        // structure of the uploaded .linst file.
        exec(
            'php generate_tables_sql_and_testNames.php < '
            . escapeshellarg($fullpath)
        );

        if (!$this->isAdminUserConfigured()) {
            // If no adminUser is configured, automatic installation is not
            // possible, so this is the last step.
            return new \LORIS\Http\Response\JSON\OK(
                ['success' => self::UPLOAD_NO_INSTALL]
            );
        }

        // Install the instrument by directly sourcing the SQL file
        // generated by `generate_tables_sql_and_testNames.php` using bash.
        // If installation is successful, `exec` will complete
        // silently. Otherwise, it will return the exit code and error
        // messsage from MySQL. This will be stored in $result and
        // logged via LorisException.
        try {
            $table_name = \NDB_BVL_Instrument::factory(
                $this->loris,
                $instrument,
                '',
                '',
            )->table;
        } catch (\NotFound $e) {
            return (new \LORIS\Http\Response\JSON\NotFound(
                $e->getMessage()
            ));
        } catch (\Exception $e) {
            return new \LORIS\Http\Response\JSON\InternalServerError(
                $e->getMessage()
            );
        }

        $db_config = $loris->getConfiguration()->getSetting('database');
        exec(
            "mysql".
            " -h" . escapeshellarg($db_config['host']).
            " -u" . escapeshellarg($db_config['adminUser']).
            " -p" . escapeshellarg($db_config['adminPassword']).
            " " . escapeshellarg($db_config['database']).
            " < " . $this->_path . "project/tables_sql/".
            escapeshellarg($table_name . '.sql'),
            $output, // $output and $status are created automatically
            $status  // by `exec` and so need not be declared above.
        );
        // An exit code of 0 is a success and 1 means failure
        if ($status) {
            error_log(
                "The installation of $instrument.sql failed. "
                . "Either: the instrument table exists (but is not in the "
                . "test_names table), or "
                . "LORIS could not connect to the database using the "
                . "credentials supplied in the config file. "
                . "Exec output: `" . implode("\n", $output) . "`"
            );
            return new \LORIS\Http\Response\JSON\InternalServerError(
                self::UPLOAD_INSTALL_FAILED
            );
        }

        return new \LORIS\Http\Response\JSON\Created();
    }

    /**
     * SetupVariables function
     *
     * @return void
     */
    function _setupVariables()
    {
        $this->headers      = [
            'Instrument',
            'Instrument_Type',
            'Table_Installed',
            'Table_Valid',
            'Pages_Valid',
        ];
        $this->columns      = ['Test_name as Instrument'];
        $this->query        = " FROM test_names";
        $this->validFilters = [];
        $this->formToFilter = [];
    }

    /**
     * Converts this menu filter to an array of the form
     *
     * Headers => (string array)
     * Data => (array of arrays of row data)
     *
     * Suitable for serializing to JSON, CSV, etc..
     *
     * @return array
     */
    function toArray()
    {
        $data = array_map(
            function ($row) {
                $instrument = $row['Instrument'];
                $row['Instrument_Type'] = $this->checkInstrumentType($instrument);
                $row['Table_Installed'] = $this->checkTableInstalled($instrument);
                $row['Table_Valid']     = $this->checkTableValid($instrument);
                $row['Pages_Valid']     = $this->checkPagesValid($instrument);
                return $row;
            },
            $this->_getFullList()
        );

        $headers = array_map(
            function ($header) {
                return ucwords(str_replace('_', ' ', $header));
            },
            $this->headers
        );

        $MappedData = [];
        foreach ($data as $row) {
            $MappedData[] = array_values($row);
        }

        return [
            'Headers'      => $headers,
            'Data'         => $MappedData,
            'fieldOptions' => $this->fieldOptions,
            'writable'     => $this->canWriteFiles(),
            'caninstall'   => $this->isAdminUserConfigured(),
        ];
    }

    /**
     * SetFilterForm function
     *
     * @return void
     */
    function _setFilterForm()
    {
    }

    /**
     * Determine the instrument type between LINST and PHP
     *
     * @param string $instrument The instrument name
     *
     * @return string The instrument type
     */
    protected function checkInstrumentType(string $instrument): string
    {
        $linst = $this->_path . "/project/instruments/$instrument.linst";
        if (file_exists($linst)) {
            return 'Instrument Builder';
        }

        $php = $this->_path .
            "/project/instruments/NDB_BVL_Instrument_$instrument.class.inc";

        if (file_exists($php)) {
            return 'PHP';
        }

        return 'Missing';
    }

    /**
     * Checks if entries exists in the subtest table for each pages of the
     * instrument's linst file.
     *
     * @param string $instrument The instrument name
     *
     * @return string An evocative answer
     */
    protected function checkPagesValid(string $instrument): string
    {
        $filename = $this->_path . "/project/instruments/$instrument.linst";
        if (!file_exists($filename)) {
            return '?';
        } else {
            return $this->checkPages($instrument);
        }
    }

    /**
     * Checks if a table with that instrument name exists
     *
     * @param string $instrument The instrument name
     *
     * @return string An evocative answer
     */
    protected function checkTableValid(string $instrument): string
    {
        $filename = $this->_path . "/project/instruments/$instrument.linst";
        if (!file_exists($filename)) {
            return '?';
        } else {
            return $this->checkTable($instrument);
        }
    }

    /**
     * Checks if a table with that instrument name exists
     *
     * @param string $instrument The instrument name
     *
     * @return string An evocative answer
     */
    protected function checkTableInstalled(string $instrument): string
    {
        // This should also check that all the columns exist and
        // have the right type, for new style instruments
        $exists = $this->_factory->database()->pselectOne(
            '
           SELECT count(*)
           FROM information_schema.tables
           WHERE TABLE_SCHEMA=:dbname AND
           TABLE_NAME=:tablename
          ',
            [
                'dbname'    => $this->_factory->settings()->dbName(),
                'tablename' => $instrument,
            ]
        );
        return ($exists > 0) ? 'Exists' : 'Missing';
    }

    /**
     * Checks the test_names table for the existence of the instrument
     *
     * @param string $instrument The instrument name
     *
     * @return bool True if the name is found in the table
     */
    protected function instrumentExists(string $instrument): bool
    {
        $count = $this->_factory->database()->pselectOne(
            'SELECT count(*) FROM test_names WHERE Test_name=:v_instrument',
            [':v_instrument' => $instrument]
        );
        return $count > 0;
    }

    /**
     * CheckType function
     *
     * @param string $tablename  the value of table name
     * @param string $columnname the value of column name
     * @param string $type       the value of the type
     *
     * @return ?string
     */
    protected function checkType($tablename, $columnname, $type)
    {
        $db           = $this->_factory->database();
        $sqlSelectOne = "SELECT count(*)".
                        " FROM information_schema.columns".
                        " WHERE TABLE_SCHEMA=:dbname".
                        " AND TABLE_NAME=:tablename".
                        " AND COLUMN_NAME=:columnname".
                        " AND DATA_TYPE=:typename";
        $exists       = $db->pselectOne(
            $sqlSelectOne,
            [
                'dbname'     => $this->_factory->settings()->dbName(),
                'columnname' => $columnname,
                'tablename'  => $tablename,
                'typename'   => $type,
            ]
        );
        if (!$exists) {
            return "Column $columnname invalid";
        }
        return null;
    }

    /**
     * CheckTable function
     *
     * @param string $instname the value of instname
     *
     * @return string An evocative answer
     */
    protected function checkTable($instname): string
    {
        $filename = $this->_path . "project/instruments/$instname.linst";
        $fp       = fopen($filename, "r");
        $db       = $this->_factory->database();

        while (($line = fgets($fp, 4096)) !== false) {
            $pieces = explode("{@}", $line);
            $type   = $pieces[0];
            $name   = $pieces[1];
            if ($name == 'Examiner') {
                continue;
            }
            switch ($type) {
            case 'page':
            case 'table':
            case 'title':
                break;
            case 'selectmultiple': // fallthrough, both selectmultiple and text
                // require varchar to save
            case 'text':
                $error = $this->checkType($instname, $name, 'varchar');
                if ($error == null) {
                    break;
                }
                return $error;
            case 'textarea':
                $error = $this->checkType($instname, $name, 'text');
                if ($error == null) {
                    break;
                }
                return $error;
            case 'date':
                $error = $this->checkType($instname, $name, 'date');
                if ($error == null) {
                    break;
                }
                return $error;
            case 'select':
                // Enums can't just check the type, they also need to
                // check the values in the enum
                $enums        = explode("{-}", $pieces[3]);
                $sqlSelectOne = "SELECT COLUMN_TYPE".
                                " FROM information_schema.columns".
                                " WHERE TABLE_SCHEMA=:dbname".
                                " AND TABLE_NAME=:tablename".
                                " AND COLUMN_NAME=:columnname".
                                " AND DATA_TYPE='enum'";
                $db_enum      = $db->pselectOne(
                    $sqlSelectOne,
                    [
                        'dbname'     => $this->_factory->settings()->dbName(),
                        'columnname' => $name,
                        'tablename'  => $instname,
                    ]
                );
                $options      = [];
                foreach ($enums as $enum) {
                    $enum_split = explode("=>", $enum);
                    $key        = $enum_split[0];
                    if ($key == 'NULL') {
                        continue;
                    } else {
                        $options[] = $key;
                    }
                }
                if ('enum(' . join(",", $options) . ')' !== $db_enum) {
                    return "$name enum invalid";
                }
            default:
                break;
            }
        }

        return "Appears Valid";
    }

    /**
     * CheckPages function
     *
     * @param string $instname the value of instname
     *
     * @return string An evocative response
     */
    protected function checkPages($instname): string
    {
        $filename = $this->_path . "project/instruments/$instname.linst";
        $fp       = fopen($filename, "r");
        $db       = $this->_factory->database();

        while (($line = fgets($fp, 4096)) !== false) {
            $pieces       = explode("{@}", $line);
            $type         = $pieces[0];
            $sqlSelectOne = "SELECT count(*)".
                            " FROM instrument_subtests".
                            " WHERE Test_name=:testname".
                            " AND Description=:testdesc";
            switch ($type) {
            case 'page':
                $exists = $db->pselectOne(
                    $sqlSelectOne,
                    [
                        'testname' => $instname,
                        'testdesc' => trim($pieces[2]),
                    ]
                );
                if ($exists <= 0) {
                    return "Missing page '" . trim($pieces[2]) . "'";
                }
            default:
                break;
            }
        }
        return 'Appears Valid';
    }

    /**
     * Return whether the adminUser is properly configured, ie. credentials are
     * set and are valid.
     * The adminUser is a MySQL user with CREATE table permissions.
     * `empty` is used instead of `isset` as blank values in the config file
     * are still considered set.
     *
     * @return bool True if a adminUser is configured properly. False if not.
     */
    protected function isAdminUserConfigured() : bool
    {
        $db        = $this->_factory->database();
        $db_config = $this->_factory->config()->getSetting('database');

        $credentials_set = !empty($db_config['adminUser'])
            && !empty($db_config['adminPassword']);
        if (!$credentials_set) {
            return false;
        }
        // Check if supplied credentials are valid by making a DB connection.
        // If the credentials are invalid, an error message will be logged to
        // the backend.
        try {
            $dbname = $db_config['database'];
            putenv("LORIS_{$dbname}_USERNAME=" . $db_config['adminUser']);
            putenv("LORIS_{$dbname}_PASSWORD=" . $db_config['adminPassword']);
            putenv("LORIS_{$dbname}_HOST=" . $db_config['host']);
            $connected = $db->connect(
                $dbname,
                false
            );
            putenv("LORIS_{$dbname}_USERNAME=");
            putenv("LORIS_{$dbname}_PASSWORD=");
            putenv("LORIS_{$dbname}_HOST=");
        } catch (\DatabaseException $e) {
            $connected = false;
        }
        return $connected;
    }

    /**
     * Checks if the instruments and table_sql are writable
     *
     * @return bool True if both directories are writable
     */
    protected function canWriteFiles(): bool
    {
        $instrument_dir = $this->_path . 'project/instruments';
        $table_dir      = $this->_path . 'project/tables_sql';

        return is_writable($instrument_dir) && is_writable($table_dir);
    }


    /**
     * Adds React table related dependencies to menu filters, since forms don't
     * usually have tables or pagination
     *
     * @return array of javascript files to be sourced
     */
    public function getJSDependencies(): array
    {
        $depends = parent::getJSDependencies();
        $baseURL = $this->_factory->settings()->getBaseURL();
        return array_merge(
            $depends,
            [
                $baseURL . '/instrument_manager/js/instrumentManagerIndex.js',
            ]
        );
    }
}
