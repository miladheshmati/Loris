<?php
/**
 * This class implements a menu filter which acts as a front end to the
 * mri_protocol_check_violations table. The table logs violations to
 * specific MRI protocol headers on a 1 header violation per line basis,
 * after the protocol type has been identified by the mri_protocol table.
 *
 * Inability to determine the scan type is logged in the mri_protocol_violations
 * table to which the NDB_Menu_Filter_mri_protocol_violations provides a frontend.
 *
 * PHP Version 7
 *
 * @category MRI
 * @package  Main
 * @author   Dave MacFarlane <driusan@bic.mni.mcgill.ca>
 * @license  Loris license
 * @link     https://www.github.com/aces/Loris-Trunk/
 */
namespace LORIS\mri_violations;
/**
 * Implements the menu filter for mri_violations_log table
 *
 * @category MRI
 * @package  Main
 * @author   Dave MacFarlane <driusan@bic.mni.mcgill.ca>
 * @license  Loris license
 * @link     https://www.github.com/aces/Loris-Trunk/
 */
class Mri_Protocol_Check_Violations extends \NDB_Menu_Filter
{
    /**
     * Determine who has access to this page. At the moment, tied to
     * access to the Mri_Violation module.
     *
     * @param \User $user The user whose access is being checked
     *
     * @return bool true if access is permitted
     */
    function _hasAccess(\User $user) : bool
    {
        return $user->hasAnyPermission(
            [
                'violated_scans_view_allsites',
                'violated_scans_view_ownsite'
            ]
        );
    }

    /**
     * Set up the class and smarty variables to use for the menu filter
     *
     * @return void
     */
    function _setupVariables()
    {
        // Find which sites the user has access to
        $user            = \User::singleton();
        $accessibleSites = $user->hasPermission('violated_scans_view_allsites')
            ? \Utility::getSiteList() : \User::singleton()->getStudySites();

        // Build the component of the WHERE clause (for the query below)
        // that will be used to restrict the search to the data associated
        // to sites the user has access to
        $siteClauseElements = [];
        $db = $this->loris->getDatabaseConnection();
        foreach (array_keys($accessibleSites) as $siteID) {
            $siteClauseElements[] = $db->quote($siteID);
        }
        $siteClause = sprintf(
            "(p.CenterID IS NULL OR p.CenterID IN (%s))",
            implode(',', $siteClauseElements)
        );

        $this->query    = " FROM mri_violations_log l 
            LEFT JOIN mri_scan_type m
            ON m.ID=l.Scan_type 
            LEFT JOIN mri_protocol_checks_group mpcg 
            ON (mpcg.MriProtocolChecksGroupID = l.MriProtocolChecksGroupID)
            LEFT JOIN candidate c ON (l.CandID=c.CandID)
            LEFT JOIN session s ON (
              l.Visit_label = s.Visit_label AND l.CandID=s.CandID
            )
            LEFT JOIN psc p ON (p.CenterID = s.CenterID)
            WHERE $siteClause";
        $this->columns  = [
            'l.PatientName',
            'l.CandID',
            'l.Visit_label',
            'm.Scan_type',
            'mpcg.Name',
            'l.Severity',
            'l.Header',
            'l.Value',
            'l.ValidRange',
            'l.ValidRegex',
            'l.SeriesUID',
            'l.TarchiveID as TarchiveID',
        ];
        $this->order_by = 'PatientName';
        $this->headers  = [
            'PatientName',
            'CandID',
            'Visit_label',
            'Scan_type',
            'MRI_Protocol_Checks_Group',
            'Severity',
            'Header',
            'Value',
            'ValidRange',
            'ValidRegex',
            'SeriesUID',
            'TarchiveID'
        ];
        $this->tpl_data['hiddenHeaders'] = json_encode(['TarchiveID']);
        $this->validFilters = [
            'l.TarchiveID',
            'l.SeriesUID',
            'l.PatientName',
            'l.CandID',
        ];
        $this->formToFilter = [
            'TarchiveID'  => 'l.TarchiveID',
            'SeriesUID'   => 'l.SeriesUID',
            'PatientName' => 'l.PatientName',
            'CandID'      => 'l.CandID',
        ];
        $this->tpl_data['hiddenHeaders'] = json_encode(['TarchiveID']);
    }

    /**
     * Does the setup required for this page. By default, sets up elements
     * that are common to every type of page. May be overridden by a specific
     * page or specific page type.
     *
     * @return void
     */
    function setup()
    {
        parent::setup();

        $this->addBasicText(
            'TarchiveID',
            'Tarchive ID:',
            [],
            [
                "size"      => 9,
                "maxlength" => 20,
            ]
        );
        $this->addBasicText(
            'PatientName',
            'PatientName:',
            [],
            [
                "size"      => 20,
                "maxlength" => 64,
            ]
        );
        $this->addBasicText(
            'CandID',
            'CandID:',
            [],
            [
                "size"      => 20,
                "maxlength" => 6,
            ]
        );
        $this->addBasicText(
            'SeriesUID',
            'DICOM Series UID:',
            [],
            [
                "size"      => 20,
                "maxlength" => 64,
            ]
        );

    }


    /**
     * Overwrite the function that is used for generating the table
     * because some fields that came from the select statement aren't
     * for display to the user, but are just because we need the value
     * to generate a link to the proper page.
     *
     * @param integer $count The offset that this page is starting at
     *
     * @return void (but as a side-effect populates $this->tpl_data['items'])
     */
    function _setDataTableRows($count)
    {
        $x = 0;
        foreach ($this->list as $item) {
            $this->tpl_data['items'][$x][0]['value'] = $x + $count;

            //print out data rows
            $i = 1;
            foreach ($item as $key => $val) {
                if ($key == 'TarchiveID') {
                    $this->tpl_data['items'][$x]['TarchiveID'] = $val;
                    continue;
                }
                if ($key == 'Severity') {
                    $this->tpl_data['items'][$x]['severity'] = $val;
                }
                $row           = [
                    'value' => $val,
                    'name'  => $key,
                ];
                $row['CandID'] = isset($item['CandID']) ? $item['CandID'] : '';
                $row['CommentID'] = isset($item['CommentID'])
                    ? $item['CommentID']
                    : '';
                $row['SessionID'] = isset($item['SessionID'])
                    ? $item['SessionID']
                    : '';

                $this->tpl_data['items'][$x][$i] = $row;
                $i++;
            }
            $x++;
        }
    }
    /**
     * Gathers JS dependecies and merge them with the parent
     *
     * @return array of javascript to be inserted
     */
    function getJSDependencies()
    {
        $factory         = \NDB_Factory::singleton();
        $baseurl         = $factory->settings()->getBaseURL();
        $depends         = parent::getJSDependencies();
        $columnFormatter = "/mri_violations/js/".
                           "mri_protocol_check_violations_columnFormatter.js";
        return array_merge(
            $depends,
            [
                $baseurl . $columnFormatter,
                $baseurl . "/mri_violations/js/mri_protocol_check_violations.js",
            ]
        );
    }
}
