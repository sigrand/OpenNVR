<?php
class timezones {
	private $timezones = array();
	public function __construct() { 
		$this->timezones = array(
			'-12' => Yii::t('timezone', '[UTC - 12] Baker Island Time'),
			'-11' => Yii::t('timezone', '[UTC - 11] Niue Time, Samoa Standard Time'),
			'-10' => Yii::t('timezone', '[UTC - 10] Hawaii-Aleutian Standard Time, Cook Island Time'),
			'-9.5' => Yii::t('timezone', '[UTC - 9:30] Marquesas Islands Time'),
			'-9' => Yii::t('timezone', '[UTC - 9] Alaska Standard Time, Gambier Island Time'),
			'-8' => Yii::t('timezone', '[UTC - 8] Pacific Standard Time'),
			'-7' => Yii::t('timezone', '[UTC - 7] Mountain Standard Time'),
			'-6' => Yii::t('timezone', '[UTC - 6] Central Standard Time'),
			'-5' => Yii::t('timezone', '[UTC - 5] Eastern Standard Time'),
			'-4.5' => Yii::t('timezone', '[UTC - 4:30] Venezuelan Standard Time'),
			'-4' => Yii::t('timezone', '[UTC - 4] Atlantic Standard Time'),
			'-3.5' => Yii::t('timezone', '[UTC - 3:30] Newfoundland Standard Time'),
			'-3' => Yii::t('timezone', '[UTC - 3] Amazon Standard Time, Central Greenland Time'),
			'-2' => Yii::t('timezone', '[UTC - 2] Fernando de Noronha Time, South Georgia &amp; the South Sandwich Islands Time'),
			'-1' => Yii::t('timezone', '[UTC - 1] Azores Standard Time, Cape Verde Time, Eastern Greenland Time'),
			'+0' => Yii::t('timezone', '[UTC] Western European Time, Greenwich Mean Time'),
			'+1' => Yii::t('timezone', '[UTC + 1] Central European Time, West African Time'),
			'+2' => Yii::t('timezone', '[UTC + 2] Eastern European Time, Central African Time'),
			'+3' => Yii::t('timezone', '[UTC + 3] Moscow Standard Time, Eastern African Time'),
			'+3.5' => Yii::t('timezone', '[UTC + 3:30] Iran Standard Time'),
			'+4' => Yii::t('timezone', '[UTC + 4] Gulf Standard Time, Samara Standard Time'),
			'+4.5' => Yii::t('timezone', '[UTC + 4:30] Afghanistan Time'),
			'+5' => Yii::t('timezone', '[UTC + 5] Pakistan Standard Time, Yekaterinburg Standard Time'),
			'+5.5' => Yii::t('timezone', '[UTC + 5:30] Indian Standard Time, Sri Lanka Time'),
			'+5.75' => Yii::t('timezone', '[UTC + 5:45] Nepal Time'),
			'+6' => Yii::t('timezone', '[UTC + 6] Bangladesh Time, Bhutan Time, Novosibirsk Standard Time'),
			'+6.5' => Yii::t('timezone', '[UTC + 6:30] Cocos Islands Time, Myanmar Time'),
			'+7' => Yii::t('timezone', '[UTC + 7] Indochina Time, Krasnoyarsk Standard Time'),
			'+8' => Yii::t('timezone', '[UTC + 8] Chinese Standard Time, Australian Western Standard Time, Irkutsk Standard Time'),
			'+8.75' => Yii::t('timezone', '[UTC + 8:45] Southeastern Western Australia Standard Time'),
			'+9' => Yii::t('timezone', '[UTC + 9] Japan Standard Time, Korea Standard Time, Chita Standard Time'),
			'+9.5' => Yii::t('timezone', '[UTC + 9:30] Australian Central Standard Time'),
			'+10' => Yii::t('timezone', '[UTC + 10] Australian Eastern Standard Time, Vladivostok Standard Time'),
			'+10.5' => Yii::t('timezone', '[UTC + 10:30] Lord Howe Standard Time'),
			'+11' => Yii::t('timezone', '[UTC + 11] Solomon Island Time, Magadan Standard Time'),
			'+11.5' => Yii::t('timezone', '[UTC + 11:30] Norfolk Island Time'),
			'+12' => Yii::t('timezone', '[UTC + 12] New Zealand Time, Fiji Time, Kamchatka Standard Time'),
			'+12.75' => Yii::t('timezone', '[UTC + 12:45] Chatham Islands Time'),
			'+13' => Yii::t('timezone', '[UTC + 13] Tonga Time, Phoenix Islands Time'),
			'+14' => Yii::t('timezone', '[UTC + 14] Line Island Time'),
			);
}

public function getTimezones() {
	return $this->timezones;
}
}
?>