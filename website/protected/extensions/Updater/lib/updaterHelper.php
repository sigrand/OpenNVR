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

	public static function update($file, $mode = 'sql') { // TODO: add downgrade available flag
		if($mode == 'sql') {
			if(strstr($file, '_mig')) {
				include(TMP.$file);
				$file = str_replace(array('sql_', '_mig', '.php'), '', $file);
				$updater = new $file();
				return $updater->safeUp();
			} else {
				$sql = file_get_contents(TMP.$file);
				$result = Yii::app()->db->createCommand($sql)->execute();
				return $result === '0' ? true : ($result ? $result : false);
			}
		} else {
			return self::filesUpdate(json_decode(trim(file_get_contents(TMP.$file)), 1));
		}
	}

	public static function changeVersion($mode, $ver = '') {
		if($mode == 'sql') {
			$version = Settings::model()->findByAttributes(array('option' => 'SQLversion'));
		} else {
			$version = Settings::model()->findByAttributes(array('option' => 'version'));
		}
		if($ver != '') {
			self::save(__CLASS__, $mode.'ver', $version->value);
		} else {
			$ver = self::get(__CLASS__, $mode.'ver');
		}
		$version->value = $ver;
		return $version->save();
	}

	private static function filesUpdate($data) {
		$version = $data['version'];
		$data = $data['data'];
		foreach($data as $zip) {
			if(preg_match('/\{YiiPath:(.*?)\}/si', $zip['name'], $path)) {
				$dir = Yii::getPathOfAlias($path[1]).'/';
				if(file_exists($dir)) {
					$zip['name'] = preg_replace('/\{YiiPath:(.*?)\}/si', $dir, $zip['name']);
				} elseif(!mkdir($dir)) {
					Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Can`t create dir: {dir}', array('{dir}' => $dir))));
					return;
				}
			}
			$zip['name'] = str_replace(array('{tmp}', '{dir}'), array(TMP, DIR), $zip['name']);
			if((file_exists($zip['name']) && is_writeable($zip['name'])) || (!file_exists($zip['name']) && is_writeable(dirname($zip['name'])))) {
				file_put_contents($zip['name'], base64_decode($zip['data']));
			} else {
				Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Can`t create file: {file}', array('{file}' => $zip['name']))));
				return;
			}
		}
		$model = Settings::model()->findByAttributes(array('option' => 'version'));
		$model->value = $version;
		$model->save();
		return true;
	}

	public static function lastCheck() {
		if(file_exists(TMP.'versions.json')) {
			return date('d-m-Y H:i:s', filemtime(TMP.'versions.json'));
		}
	}
}
?>