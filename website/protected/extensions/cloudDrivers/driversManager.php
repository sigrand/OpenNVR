<?php
class driversManager {
	public function init() {
		$http = new http;
		//$http->setProxy('localhost', 666);
		//$http->setTimeout(10);
		if(!is_writeable(DIR)) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Error make dir writeable: '.DIR)));
			return false;
		}
		$repo = Settings::model()->getValue('repo');
		if($repo && !empty($repo)) {
			$this->driver = new dropWebLight($repo, $http);
			return true;
		} elseif($repo === false) {
			$model = new Settings;
			$model->option = 'repo';
			$model->value = '';
			$model->save();
		}
	}

	public function getVersions($useCache = 0) {
		if($useCache) {
			if(file_exists(DIR.'versions.json')) {
				return json_decode(file_get_contents(DIR.'versions.json'), 1);
			} else {
				return false;
			}
		}
		if(isset($this->driver)) {
			$files = $this->driver->getFileList();
			if(in_array('versions.json', array_keys($files))) {
				$this->driver->get('versions.json', DIR);
				return json_decode(file_get_contents(DIR.'versions.json'), 1);
			}
		}
	}
}
?>