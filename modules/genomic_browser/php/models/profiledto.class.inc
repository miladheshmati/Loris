<?php declare(strict_types=1);
/**
 * PHP version 7
 *
 * @category Genomic_Browser
 * @package  Main
 * @author   Xavier Lecours <xavier.lecours@mcin.ca>
 *           Alizée Wickenheiser <alizee.wickenheiser@mcin.ca>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://github.com/aces/Loris-Trunk
 */

namespace LORIS\genomic_browser\Models;

/**
 * A ProfileDTO represents an profile in the genomic browser module.
 * It is doing the mapping between the profile table columns
 * and the JSON object properties returned to the frontend.
 *
 * Additionally, it implements the DataInstance interface so it can be used by
 * a Database Provisioner.
 *
 * @category Genomic_Browser
 * @package  Main
 * @author   Xavier Lecours <xavier.lecours@mcin.ca>
 *           Alizée Wickenheiser <alizee.wickenheiser@mcin.ca>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://github.com/aces/Loris-Trunk
 */
class ProfileDTO implements \LORIS\Data\DataInstance
{

    /**
     * The centerID
     *
     * @var string
     */
    private $_centerID = "";

    /**
     * The projectID
     *
     * @var string
     */
    private $_projectID = "";

    /**
     * The PSC name
     *
     * @var string
     */
    private $_PSC = "";

    /**
     * The DCCID
     *
     * @var string
     */
    private $_DCCID = "";

    /**
     * The PSCID
     *
     * @var string
     */
    private $_PSCID = "";

    /**
     * The Sex
     *
     * @var string
     */
    private $_Sex = "";

    /**
     * The Cohort
     *
     * @var string
     */
    private $_Cohort = "";

    /**
     * The DoB
     *
     * @var string
     */
    private $_DoB = "";

    /**
     * The externalID
     *
     * @var string
     */
    private $_externalID = "";

    /**
     * The File
     *
     * @var string
     */
    private $_File = "";

    /**
     * The SNP
     *
     * @var string
     */
    private $_SNP = "";

    /**
     * The CNV
     *
     * @var string
     */
    private $_CNV = "";

    /**
     * The CPG
     *
     * @var string
     */
    private $_CPG = "";

    /**
     * Implements \LORIS\Data\DataInstance interface for this row.
     *
     * @return array
     */
    public function jsonSerialize(): array
    {
        return [
            'centerID'   => $this->_PSC,
            'DCCID'      => $this->_DCCID,
            'PSCID'      => $this->_PSCID,
            'Sex'        => $this->_Sex,
            'Cohort'     => $this->_Cohort,
            'DoB'        => $this->_DoB,
            'externalID' => $this->_externalID,
            'File'       => $this->_File,
            'SNP'        => $this->_SNP,
            'CNV'        => $this->_CNV,
            'CPG'        => $this->_CPG,
        ];
    }

    /**
     * Returns the CenterID for this row
     *
     * @return integer The CenterID
     */
    public function getCenterID(): int
    {
        return (int) $this->_centerID;
    }

    /**
     * Returns the ProjectID for this row
     *
     * @return integer The ProjectID
     */
    public function getProjectID(): int
    {
        return (int) $this->_projectID;
    }
}
