<?php
/**
 * XDetectMobileBrowser class file.
 *
 * @author Marco van 't Wout <marco@tremani.nl>
 * @copyright Copyright &copy; Tremani, 2012
 */

/**
 * Handles detecting mobile browsers.
 * Results are stored locally for caching calls within the same request, and
 * stored in a cookie for caching across requests.
 * Detection regex used from http://detectmobilebrowsers.com/
 *
 * Install as an application component, in your config:
 *
 * 'components' => array(
 *    'detectMobileBrowser' => array(
 *        'class' => 'ext.yii-detectmobilebrowser.XDetectMobileBrowser',
 *     ),
 * ),
 *
 * You can get the current user preference like this:
 *
 * if (Yii::app()->detectMobileBrowser->showMobile) {
 *     // do something
 * }
 *
 * By default it will use the automatically detected value.
 * You can also set the preference yourself like this:
 *
 * public function actionShowMobile() {
 *     Yii::app()->detectMobileBrowser->showMobile = true;
 *     $this->redirect(array('/site/index'));
 * }
 *
 */
class XDetectMobileBrowser extends CApplicationComponent
{
    /**
     * Cookie name for storing user preference
     */
    const COOKIE_NAME_SHOW = 'showMobile';

	/**
     * Cookie name for storing detected result
     */
    const COOKIE_NAME_IS = 'isMobile';
 
    /**
     * @var boolean show mobile version to the client?
     */
    protected $_showMobile;

	/**
     * @var boolean is mobile device detected?
     */
    protected $_isMobile;
 
    /**
     * Check user preference for mobile.
     * Store result in class var and cookie.
     */
    public function getShowMobile()
    {
        if (!isset($this->_showMobile)) {
            $cookie = Yii::app()->request->cookies[self::COOKIE_NAME_SHOW];
            if (isset($cookie)) {
                $this->_showMobile = (bool)$cookie->value; // int to bool
            } else {
                $this->setShowMobile($this->getIsMobile());
            }
        }
        return $this->_showMobile;
    }
 
    /**
     * Set user preference for showMobile.
	 * Store result in class var and cookie.
     * @param bool $value
     */
    public function setShowMobile($value)
    {
        $this->resetShowMobile();
        $this->_showMobile = (bool)$value;
        $this->setCookie(self::COOKIE_NAME_SHOW, $this->_showMobile);
    }
	
	/**
     * Detect if user is mobile.
     * Store result in class var and cookie.
     */
    public function getIsMobile()
    {
        if (!isset($this->_isMobile)) {
            $cookie = Yii::app()->request->cookies[self::COOKIE_NAME_IS];
            if (isset($cookie)) {
                $this->_isMobile = (bool)$cookie->value; // int to bool
            } else {
                $this->_isMobile = $this->isMobileBrowser();
                $this->setCookie(self::COOKIE_NAME_IS, $this->_isMobile);
            }
        }
        return $this->_isMobile;
    }
	
	/**
     * Clears the cookie for user preference.
     */
    public function resetShowMobile()
    {
        unset(Yii::app()->request->cookies[self::COOKIE_NAME_SHOW]);
	}
	
	/**
     * Clears the cookie for detected value.
     */
    public function resetIsMobile()
    {
		unset(Yii::app()->request->cookies[self::COOKIE_NAME_IS]);
    }
 
    /**
     * Set cookie for storing isMobile
	 * @var string cookie name
	 * @var boolean value (must be stored as int)
     */
    protected function setCookie($cookieName, $value)
    {
        $cookie = new CHttpCookie($cookieName, (int)$value); // bool to int
        $cookie->expire = time()+(3600*24*365); //1 year
        $cookie->path = Yii::app()->baseUrl . '/';
        Yii::app()->request->cookies[$cookieName] = $cookie;
    }
 
    /**
	 * Performs a regexp check on the User Agent string to determine if this is a mobile browser.
	 * I kindly asked them to put their code on GitHub, but no response.
	 * Last updated: 18-10-2012
	 * @see http://detectmobilebrowsers.com/
	 * @return bool is mobile
	 */
	protected function isMobileBrowser()
	{
		$useragent=$_SERVER['HTTP_USER_AGENT'];
		return preg_match('/android|ipad|playbook|silk|(android|bb\d+|meego).+mobile|avantgo|bada\/|blackberry|blazer|compal|elaine|fennec|hiptop|iemobile|ip(hone|od)|iris|kindle|lge |maemo|midp|mmp|netfront|opera m(ob|in)i|palm( os)?|phone|p(ixi|re)\/|plucker|pocket|psp|series(4|6)0|symbian|treo|up\.(browser|link)|vodafone|wap|windows (ce|phone)|xda|xiino/i',$useragent)||preg_match('/1207|6310|6590|3gso|4thp|50[1-6]i|770s|802s|a wa|abac|ac(er|oo|s\-)|ai(ko|rn)|al(av|ca|co)|amoi|an(ex|ny|yw)|aptu|ar(ch|go)|as(te|us)|attw|au(di|\-m|r |s )|avan|be(ck|ll|nq)|bi(lb|rd)|bl(ac|az)|br(e|v)w|bumb|bw\-(n|u)|c55\/|capi|ccwa|cdm\-|cell|chtm|cldc|cmd\-|co(mp|nd)|craw|da(it|ll|ng)|dbte|dc\-s|devi|dica|dmob|do(c|p)o|ds(12|\-d)|el(49|ai)|em(l2|ul)|er(ic|k0)|esl8|ez([4-7]0|os|wa|ze)|fetc|fly(\-|_)|g1 u|g560|gene|gf\-5|g\-mo|go(\.w|od)|gr(ad|un)|haie|hcit|hd\-(m|p|t)|hei\-|hi(pt|ta)|hp( i|ip)|hs\-c|ht(c(\-| |_|a|g|p|s|t)|tp)|hu(aw|tc)|i\-(20|go|ma)|i230|iac( |\-|\/)|ibro|idea|ig01|ikom|im1k|inno|ipaq|iris|ja(t|v)a|jbro|jemu|jigs|kddi|keji|kgt( |\/)|klon|kpt |kwc\-|kyo(c|k)|le(no|xi)|lg( g|\/(k|l|u)|50|54|\-[a-w])|libw|lynx|m1\-w|m3ga|m50\/|ma(te|ui|xo)|mc(01|21|ca)|m\-cr|me(rc|ri)|mi(o8|oa|ts)|mmef|mo(01|02|bi|de|do|t(\-| |o|v)|zz)|mt(50|p1|v )|mwbp|mywa|n10[0-2]|n20[2-3]|n30(0|2)|n50(0|2|5)|n7(0(0|1)|10)|ne((c|m)\-|on|tf|wf|wg|wt)|nok(6|i)|nzph|o2im|op(ti|wv)|oran|owg1|p800|pan(a|d|t)|pdxg|pg(13|\-([1-8]|c))|phil|pire|pl(ay|uc)|pn\-2|po(ck|rt|se)|prox|psio|pt\-g|qa\-a|qc(07|12|21|32|60|\-[2-7]|i\-)|qtek|r380|r600|raks|rim9|ro(ve|zo)|s55\/|sa(ge|ma|mm|ms|ny|va)|sc(01|h\-|oo|p\-)|sdk\/|se(c(\-|0|1)|47|mc|nd|ri)|sgh\-|shar|sie(\-|m)|sk\-0|sl(45|id)|sm(al|ar|b3|it|t5)|so(ft|ny)|sp(01|h\-|v\-|v )|sy(01|mb)|t2(18|50)|t6(00|10|18)|ta(gt|lk)|tcl\-|tdg\-|tel(i|m)|tim\-|t\-mo|to(pl|sh)|ts(70|m\-|m3|m5)|tx\-9|up(\.b|g1|si)|utst|v400|v750|veri|vi(rg|te)|vk(40|5[0-3]|\-v)|vm40|voda|vulc|vx(52|53|60|61|70|80|81|83|85|98)|w3c(\-| )|webc|whit|wi(g |nc|nw)|wmlb|wonu|x700|yas\-|your|zeto|zte\-/i',substr($useragent,0,4));
	}
}