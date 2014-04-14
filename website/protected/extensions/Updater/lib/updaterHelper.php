<?php

class updaterHelper {

	public static function save($class, $option, $value, $mode = 0) {
		$data = array();
		$data[$option] = $value;
		file_put_contents(TMP.'sql_'.$class.'.json', json_encode($data));
	}

	public static function get($class, $option, $mode = 0) {
		if(!$mode) {
			$data = json_decode(file_get_contents(TMP.'sql_'.$class.'.json'), 1);
			return $data[$option];
		} else {
			$data = json_decode(file_get_contents(TMP.$class), 1);
			return $option ? $data[$option] : $data;
		}
	}

	public static function update($file) { // TODO: check for migration or plain SQL, add downgrade available flag
		if(strstr($file, '_mig')) {
			include(TMP.$file);
			$file = str_replace(array('sql_', '_mig', '.php'), '', $file);
			$updater = new $file();
			return $updater->safeUp();
		} else {
			$sql = file_get_contents(TMP.$file);
			$result = Yii::app()->db->createCommand($sql)->execute();
			return $result === '0' ? true : ($result ? $result : false); // need some tests
		}
	}
}
?>