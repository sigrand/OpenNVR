<?php
class driversManager {
	public function init() {
		$http = new http;
		//$http->setProxy('localhost', 666);
		//$http->setTimeout(10);
		if(!is_writeable(TMP)) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Error make TMP writeable: {DIR}', TMP)));
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

	public function getVersion($useCache = 0) {
		if($useCache) {
			if(file_exists(TMP.'version.json')) {
				return UpdateHelper::get('version.json', 0, 1);
			} else {
				return false;
			}
		}
		if(isset($this->driver)) {
			$files = $this->driver->getFileList();
			if(in_array('version.json', array_keys($files))) {
				$this->driver->get('version.json', TMP);
				return UpdateHelper::get('version.json', 0, 1);
			}
		}
	}

	public function getLast($what) {
		if(isset($this->driver)) {
			$files = $this->driver->getFileList();
			if(in_array('version.json', array_keys($files))) {
				$data = UpdateHelper::get('version.json', 0, 1);
				if(isset($data['files'][$what]) && !empty($data['files'][$what])) {
					if(in_array($data['files'][$what], array_keys($files))) {
						if(file_exists(TMP.$data['files'][$what])) {
							return $data['files'][$what];
						}
						if($this->driver->get($data['files'][$what], TMP)) {
							return $data['files'][$what];
						}
					}
				}
			}
		}
	}
}
?>