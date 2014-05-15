<?php

class backupHelper {
	public static $excludeFiles = array(
		'tmp.gz',
		'list.json',
		);
	public static $excludeDirs = array(
		'assets',
		'fw',
		'protected/runtime',
		'protected/extensions/Updater/backups',
		'protected/extensions/Updater/tmp',
		);

	public static function getBackupsList() {
		$list = array();
		if(file_exists(BKP.'list.json')) {
			$list = file_get_contents(BKP.'list.json');
			$list = !empty($list) ? json_decode($list, 1) : array();
		}
		return $list;
	}	

	public static function backupDelete($id) {
		$backups = self::getBackupsList();
		foreach($backups as $backup) {
			if($backup['id'] == $id) {
				if(unlink(BKP.$backup['file'])) {
					self::updateBackupsList($id);
					return 1;
				}
			}
		}
	}

	public static function makeBackup() {
		if(!is_writeable(BKP)) { return 2; }
		$id = uniqid();
		$time = time();
		$version = Settings::model()->findByAttributes(array('option' => 'version'))->value;
		$file = BKP.'backup_'.$id.'_'.str_replace('.', '_', $version).'_'.$time.'.zip';
		$base = Yii::getPathOfAlias('webroot').'/';
		$sql = self::SQLbackup();
		$zip = new ZipArchive;
		if($zip->open($file, ZipArchive::CREATE) === TRUE) {
			if($sql) {
				$zip->addFile($sql, 'LastSQLbackup.sql');
			}
			$zip = self::addDirectoryToZip($zip, $base, $base);
			$zip->close();
			self::updateBackupsList(array('id' => $id, 'time' => $time, 'version' => $version, 'file' => str_replace(BKP, '', $file), 'size' => filesize($file)));
			unlink($sql);
			return 1;
		} else {
			return 2;
		}
	}

	public static function getBackup($id) {
		$list = self::getBackupsList();
		foreach($list as $k => $backup) {
			if($backup['id'] == $id) {
				return $backup;
			}
		}
	}

	public static function addDirectoryToZip($zip, $dir, $base) {
		$newFolder = str_replace($base, '', $dir);
		$zip->addEmptyDir($newFolder);
		if(in_array(str_replace(array('/', '\\'), '', $newFolder), str_replace(array('/', '\\'), '', self::$excludeDirs))) {
			return $zip;
		}
		//foreach(array_merge(glob($dir.'/*'), glob($dir.'/.*')) as $file) {
		foreach(glob($dir.'/*') as $file) {
			if(is_dir($file)) {
				$zip = self::addDirectoryToZip($zip, $file, $base);
			} else {
				$newFile = str_replace($base, '', $file);
				if(!in_array(str_replace(array('/', '\\'), '', $newFile), self::$excludeFiles)) {
					$zip->addFile($file, $newFile);
				}
			}
		}
		return $zip;
	}

	private static function updateBackupsList($params) {
		$list = self::getBackupsList();
		if(is_array($params)) {
			$list[] = $params;
		} else { // if not array delete backup from list
			foreach($list as $k => $backup) {
				if($backup['id'] == $params) {
					unset($list[$k]);
				}
			}
		}
		return file_put_contents(BKP.'list.json', json_encode($list));
	}

	public static function SQLbackup() {
		$db = Yii::app()->db->createCommand('SELECT DATABASE()')->queryScalar();
		$tables = Yii::app()->db->createCommand('SHOW TABLES')->queryColumn();
		$version = Settings::model()->findByAttributes(array('option' => 'SQLversion'));
		$file_name = BKP.'SQL_bkp_'.trim(str_replace('.', '_', $version->value)).'_'.time().'.sql';
		$backup_file = fopen($file_name, 'w');
		$f = function($e) { return is_null($e) ? 'NULL' : Yii::app()->db->quoteValue($e); };
		if($tables && $version && $backup_file) {
			fwrite($backup_file, "\nDROP DATABASE IF EXISTS `{$db}`;\nCREATE DATABASE IF NOT EXISTS `{$db}`;\nUSE `{$db}`;\n");
			foreach($tables as $table) {
				$rows = Yii::app()->db->createCommand('SHOW CREATE TABLE '.$table)->queryRow();
				fwrite($backup_file, "\nDROP TABLE IF EXISTS `{$table}`;\n{$rows['Create Table']};\n");
				$rows = Yii::app()->db->createCommand('SELECT * FROM `'.$table.'`')->queryAll();
				$c = count($rows)-1;
				if($c >= 0) {
					fwrite($backup_file, "\nINSERT INTO `{$table}` VALUES ");
					foreach($rows as $k => $row) {
						fwrite($backup_file, '('.join(',', array_map($f, $row)).($k == $c ? ')' : '),'));
					}
					fwrite($backup_file, ";\n");
				}
			}
		}
		fclose($backup_file);
		return file_exists($file_name) ? $file_name : 0;
	}
}
?>