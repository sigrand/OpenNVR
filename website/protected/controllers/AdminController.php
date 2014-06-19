<?php

class AdminController extends Controller {
	var $userActions = array();
	public function filters() {
		return array( 
			'accessControl',
			);
	}

	public function accessRules() {
		return array(
			array('allow',
				'actions'=>array('users', 'addUser'),
				'users'=>array('@'),
				'expression' => 'Yii::app()->user->permissions == 2'
				),
			array('allow',
				'users' => array('@'),
				'expression' => 'Yii::app()->user->isAdmin'
				),
			array('deny',
				'users' => array('*'),
				),
			);
	}

	public function actionDevTools() {
		Yii::import('ext.Updater.index', 1);
		$updates = backupHelper::getBackupsList('update');
		$f = function($e) {
			$e['size'] = $this->convertSize($e['size']);
			return $e;
		};
		$updates = array_reverse(array_map($f, $updates));
		$this->render('devtools', array('updates' => $updates));
	}

	public function actionUpdateAdd() {
		Yii::import('ext.Updater.index', 1);
		$status = backupHelper::makeBackup('fullUpdate');
		if($status == 1)  {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! update created')));
		} elseif($status == 2) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Update not created. Dir is not writeable: '.BKP)));
		} elseif($status == 0) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Update not created')));
		}
		$this->redirect($this->createUrl('admin/devtools'));
	}

	public function actionUpdateDelete($id) {
		Yii::import('ext.Updater.index', 1);
		if(backupHelper::backupDelete($id)) {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! Update deleted')));		
			$this->redirect($this->createUrl('admin/devtools'));
			Yii::app()->end();
		}
		Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Update not deleted')));
		$this->redirect($this->createUrl('admin/devtools'));
	}

	public function actionUpdateDownload($id) {
		Yii::import('ext.Updater.index', 1);
		$update = backupHelper::getBackup($id, 'update');
		if(!empty($update)) {
			Yii::app()->getRequest()->sendFile($update['file'], file_get_contents(BKP.$update['file']));
		}
	}

	public function actionBackupManager() {
		Yii::import('ext.Updater.index', 1);
		$backups = backupHelper::getBackupsList();
		$f = function($e) {
			$e['size'] = $this->convertSize($e['size']);
			return $e;
		};
		$backups = array_reverse(array_map($f, $backups));
		$this->render('bumanager', array('backups' => $backups));
	}

	public function actionBackupAdd() {
		Yii::import('ext.Updater.index', 1);
		$status = backupHelper::makeBackup();
		if($status == 1)  {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! backup created')));
		} elseif($status == 2) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Backup not created. Dir is not writeable: '.BKP)));
		} elseif($status == 0) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Backup not created')));
		}
		$this->redirect($this->createUrl('admin/backupmanager'));
	}

	public function actionBackupDelete($id) {
		Yii::import('ext.Updater.index', 1);
		if(backupHelper::backupDelete($id)) {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! backup deleted')));		
			$this->redirect($this->createUrl('admin/backupmanager'));
			Yii::app()->end();
		}
		Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Backup not deleted')));
		$this->redirect($this->createUrl('admin/backupmanager'));
	}

	public function actionBackupDownload($id) {
		Yii::import('ext.Updater.index', 1);
		$backup = backupHelper::getBackup($id);
		if(!empty($backup)) {
			Yii::app()->getRequest()->sendFile($backup['file'], file_get_contents(BKP.$backup['file']));
		}
	}

	public function actionCheckUpdate() {
		$result = array();
		Yii::import('ext.Updater.index', 1);
		$d = new driversManager;
		if(!$d->init()) {
			$this->redirect($this->createUrl('admin/updater'));
			Yii::app()->end();
		}
		$version = $d->getVersion();
		if(!$version) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Update checking failed. Repo access problem')));
			$this->redirect($this->createUrl('admin/updater'));	
			Yii::app()->end();
		}
		$model = Settings::model()->findByAttributes(array('option' => 'version'));
		if($version == $model->value) {
			$result['version'] = 1;
		} else {
			$result['version'] = $version;
		}
		Yii::app()->user->setFlash('version', json_encode($result));
		Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! update checked')));
		$this->redirect($this->createUrl('admin/updater'));
	}

	public function updateSQLVersion($d) {
		$result = array();
		$version = $d->getVersion(1);
		$filename = $d->getLast('SQL');
		if($filename == 'none') { return 1; }
		if(updateHelper::update($filename)) {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! SQL updated')));
			return 1;
		} else {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Fail, cant execute SQL update file')));
		}
	}

	public function actionUpdate() {
		$result = array();
		Yii::import('ext.Updater.index', 1);
		$d = new driversManager;
		if(!$d->init()) {
			$this->redirect($this->createUrl('admin/updater'));
			Yii::app()->end();
		}
		$version = $d->getVersion(1);
		if(!$version) {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Update checking failed. Repo access problem')));
			$this->redirect($this->createUrl('admin/updater'));	
			Yii::app()->end();
		}
		$model = Settings::model()->findByAttributes(array('option' => 'version'));
		if($version == $model->value) {
			Yii::app()->user->setFlash('notify', array('type' => 'warning', 'message' => Yii::t('admin', 'No new version')));	
			$this->redirect($this->createUrl('admin/updater'));	
			Yii::app()->end();
		}
		$filename = $d->getLast('files');
		if(updateHelper::update($filename, 'files')) {
			if($this->updateSQLVersion($d)) {
				UpdateHelper::changeVersion($version);
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! updated')));
			}
		} else {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Fail, cant execute update file')));
		}
		Yii::app()->user->setFlash('version', json_encode($result));
		$this->redirect($this->createUrl('admin/updater'));
	}

	public function actionUpdater() {
		Yii::import('ext.Updater.index', 1);
		$d = new driversManager;
		$version = $d->getVersion(1);
		Yii::app()->user->setFlash('version', json_encode($version));
		$model = Settings::model()->findByAttributes(array('option' => 'version'));
		if(!$model) {
			$model = new Settings;
			$model->option = $value;
			$model->value = '0.1';
			$model->save();
		}
		$this->render('updater', array('model' => $model, 'last_check' => updateHelper::lastCheck()));
	}

	public function actionSettings() {
		if(isset($_POST['Settings'])) {
			foreach ($_POST['Settings'] as $k => $model) {
				$models[$k] = Settings::model()->findByPK($_POST['Settings'][$k]['id']);
				$models[$k]->attributes = $_POST['Settings'][$k];
				if($models[$k]->validate() && $models[$k]->save()) {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Huge success! Settings changed')));
				} else {
					Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Fail =\, settings not changed')));
				}
			}
		} else {
			$models = Settings::model()->findAll();
		}
		$this->render('settings', array('models' => $models));
	}

	public function actionServers() {
		$this->render('servers/index', array('servers' => Servers::model()->findAll()));
	}

	public function actionServerAdd() {
		$model = new Servers;
		if(isset($_POST['Servers'])) {
			$model->attributes = $_POST['Servers'];
			if($model->validate() && $model->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Server added')));
				$this->redirect(array('servers'));
			}
		}
		$this->render('servers/edit', array('model' => $model));
	}

	public function actionServerEdit($id) { // TODO add check owner
		$model = Servers::model()->findByPK($id);
		if(!$model) {
			$this->redirect(array('servers'));
		}
		if(isset($_POST['Servers'])) {
			$model->attributes = $_POST['Servers'];
			if($model->validate() && $model->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Server changed')));
				$this->redirect(array('servers'));
			} else {
				Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Server not changed')));
				$this->redirect(array('serverEdit', 'id' => $model->id));
			}
		}
		$this->render('servers/edit', array('model' => $model));
	}

	public function actionServerDelete($id) {
		$model = Servers::model()->findByPK($id);
		if(!$model) {
			$this->redirect(array('servers'));
		}
		if($model->delete()) {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Server deleted')));
		} else {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'Server not deleted')));
		}
		$this->redirect(array('servers'));
	}

	public function actionStat($type, $id) {
		Yii::import('ext.moment.index', 1);
		$momentManager = new momentManager($id);
		$stat = $momentManager->stat($type);
		$units = array(
			'packet time from ffmpeg to nvr' => Yii::t('admin', 'ms'),
			'packet time from ffmpeg to restreamer' => Yii::t('admin', 'ms'),
			'RAM utilization' => Yii::t('admin', 'Kb'),
			'CPU utilization' => Yii::t('admin', '%'),
			'HDD utilization' => Yii::t('admin', '%'),
			'rtmp_sessions' => Yii::t('admin', 'units'),
			);

		if(empty($stat)) {
			$this->render('stat/index', array('title' => Yii::t('admin', 'Statistics not avaiable'), 'stat' => array(), 'type' => $type, 'id' => $id));
			Yii::app()->end();
		}
		switch ($type) {
			case 'disk':
			$stat = json_decode($stat, 1);
			foreach($stat as $k => $s) {
				foreach ($s['disk info'] as $key => $value) {
					$s['disk info'][$key] = $this->convertSize($value);
				}
				$stat[$k] = $s;
			}
			$all = $stat;
			break;      

			case 'rtmp':
			$all = str_replace(array('<html><body>', '</html></body>'), '', $stat);
			break;        

			case 'load':
			$stat = json_decode($stat, 1);
			$all = array();
			$stat = array_reverse($stat['statistics']);
			$stat = array_slice($stat, 0, 100);
			$stat = array_reverse($stat);
			foreach($stat as $key => $value) {
				//$all['time'][] = $value['time'];
				$time = explode(' ', $value['time']);
				$all['time'][] = $time[1];
				/*
				//$time = explode(':', $time[1]);
				if(!isset($timecache[$time[0]])) {
					$timecache[$time[0]] = 1;
					$all['time'][] = $time[0].':'.($time[1] < 10 ? '0'.$time[1] : $time[1]);
				} else {
					$all['time'][] = '';
				}
				//*/
				foreach($value as $k => $v) {
					if($k == 'time') { continue; }
					if($k == 'rtmp_sessions') {
						$all['rtmp_sessions']['unit'] = $units[$k];
						$all['rtmp_sessions']['sessions'][] = (float)$v;
						continue;
					}			
					if($k == 'HDD utilization') {
						$all['hdd']['unit'] = $units[$k];
						foreach($v as $t) {
							$all['hdd'][$t['device']][] = (float)$t['util'];
						}
						continue;
					}
					$all[$k]['unit'] = $units[$k];
					$all[$k]['min'][] = (float)$v['min'];
					$all[$k]['max'][] = (float)$v['max'];
					$all[$k]['avg'][] = (float)$v['avg'];
				}
			}
			break;
			case 'source_info':
			print_r($stat);
			return true;
			break;

			default:
			$all = array();
			break;
		}

		$this->render('stat/index', array('title' => Yii::t('admin', 'Statistics(100 recent changes)'), 'stat' => $all, 'type' => $type, 'id' => $id));
	}

	function convertSize($s) {
		if (is_int($s)) {
			$s = sprintf("%u", $s);		
		} if($s >= 1073741824) {
			return sprintf('%1.2f', $s / 1073741824 ). ' GB';
		} elseif($s >= 1048576) {
			return sprintf('%1.2f', $s / 1048576 ) . ' MB';
		} elseif($s >= 1024) {
			return sprintf('%1.2f', $s / 1024 ) . ' KB';
		} else {
			return $s . ' B';
		}
		$this->render('stat', array('title' => 'Statistics(100 recent changes)', 'stat' => $all));
	}

	public function actionCams() {
		$id = Yii::app()->user->getId();
		if(isset($_POST['CamsForm']) && !empty($_POST['CamsForm']) && array_sum($_POST['CamsForm']) != 0) {
			if(isset($_POST['public'])) {
				foreach ($_POST['CamsForm'] as $key => $cam) {
					if($cam) {
						$key = explode('_', $key);
						$cam = Cams::model()->findByPK((int)$key[1]);
						if($cam) {
							$cam->is_public = $cam->is_public ? 0 : 1;
							if($cam->save()) {
								Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Cams settings successfully changed')));
							}
						}
					}
				}
			}
		}
		$myCams = Cams::model()->findAllByAttributes(array('user_id' => $id));
		$public = Cams::model()->findAllByAttributes(array('is_public' => 1));
		$criteria = new CDbCriteria();
		$criteria->addNotInCondition('id', CHtml::listData($myCams, 'id', 'id'));
		$criteria->addNotInCondition('id', CHtml::listData($public, 'id', 'id'));
		$count = Cams::model()->count($criteria);
		$pages = new CPagination($count);
		$pages->pageSize = 10;
		$pages->applyLimit($criteria);
		$all = Cams::model()->findAll($criteria);
		$this->render(
			'cams/index',
			array(
				'form' => new CamsForm,
				'myCams' => $myCams,
				'publicCams' => $public,
				'allCams' => $all,
				'pages' => $pages
				)
			);
	}

	public function actionUsers() {
		if(isset($_POST['UsersForm']) && !empty($_POST['UsersForm']) && array_sum($_POST['UsersForm']) != 0) {
			foreach ($_POST['UsersForm'] as $key => $user) {
				if($user) {
					$key = explode('_', $key);
					$user = Users::model()->findByPK((int)$key[1]);
					if($user) {
						$this->userAction($_POST, $user);
					}
				}
			}
		}
		$admins = Users::model()->findAllByAttributes(array('status' => 3));
		$operators = Users::model()->findAllByAttributes(array('status' => 2));
		$viewers = Users::model()->findAllByAttributes(array('status' => 1));
		$banned = Users::model()->findAllByAttributes(array('status' => 4));
		$criteria = new CDbCriteria();
		$criteria->addNotInCondition('id', CHtml::listData($admins, 'id', 'id'));
		$criteria->addNotInCondition('id', CHtml::listData($banned, 'id', 'id'));
		$criteria->addNotInCondition('id', CHtml::listData($operators, 'id', 'id'));
		$criteria->addNotInCondition('id', CHtml::listData($viewers, 'id', 'id'));
		$count = Users::model()->count($criteria);
		$pages = new CPagination($count);
		$pages->pageSize = 10;
		$pages->applyLimit($criteria);
		$all = Users::model()->findAll($criteria);
		$this->render(
			Yii::app()->user->permissions == 2 ? 'users/oper' : 'users/index',
			array(
				'form' => new UsersForm,
				'admins' => $admins,
				'operators' => $operators,
				'viewers' => $viewers,
				'banned' => $banned,
				'all' => $all,
				'pages' => $pages
				)
			);
	}

	private function userAction($actions, $user) {
		
		$this->userActions = array(
			'ban' => array(4, Yii::t('admin', 'banned')),
			'unban' => array(1, Yii::t('admin', 'unbanned')),
			'levelup' => array(3, Yii::t('admin', 'up')),
			'active' => array(1, Yii::t('admin', 'activated')),
			'dismiss' => array(1, Yii::t('admin', 'down')),
			);
		foreach($this->userActions as $key => $value) {
			if(isset($actions[$key])) {
				if($key == 'levelup' && Yii::app()->user->permissions == 3) {
					$user->status++;
					if($user->status >= 2) {
						Yii::import('ext.moment.index', 1);
						$momentManager = new momentManager($user->server_id);
						$momentManager->addQuota($user->id, Settings::getValue('quota_size'));
					}
				} elseif($key == 'dismiss' && Yii::app()->user->permissions == 3) {
					if($user->status >= 2 && $user->status-1 <= 2) {
						Yii::import('ext.moment.index', 1);
						$momentManager = new momentManager($user->server_id);
						$momentManager->removeQuota($user->id, Settings::getValue('quota_size'));
					}
					$user->status--;
				} else {
					$user->status = $value[0];
				}
				if($user->save()) {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'Users {action}', array('{action}' => $value[1]))));
					return;
				}
			}
		}	
	}

	public function actionLogs($type) {
		if($type == 'system') {
			$logs = Notifications::model()->findAllByAttributes(array('creator_id' => 0), array('order' => 'time DESC'));
		} else {
			$logs = Notifications::model()->findAll(array('condition' => 'creator_id > 0', 'order' => 'time DESC'));
		}
		$this->render('logs/index', array('type' => $type, 'logs' => $logs));
	}

	public function actionAddUser() {
		$model = new UserForm;
		if(isset($_POST['UserForm'])) {
			$model->attributes = $_POST['UserForm'];
			if($model->validate() && $model->register()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'User added')));
				$this->redirect(array('users'));
			} else {
				Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'User not added')));
			}
		}
		$servers = array('0' => 'none');
		$server = Servers::model()->findAll(array('select' => 'id, ip, comment'));
		foreach($server as $s) {
			$servers[$s->id] = $s->ip.($s->comment ? ' [ '.$s->comment.' ]' : '');
		}
		$this->render('users/edit', array('model' => $model, 'servers' => $servers));
	}
	
	public function actionEditUser($id) {
		$model = new UserForm;
		$model->load($id);
		if(isset($_POST['UserForm'])) {
			$model->attributes = $_POST['UserForm'];
			if($model->validate() && $model->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('admin', 'User edited')));
				$this->redirect(array('users'));
			} else {
				Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('admin', 'User not edited')));
			}
		}
		$servers = array('0' => 'none');
		$server = Servers::model()->findAll(array('select' => 'id, ip, comment'));
		foreach($server as $s) {
			$servers[$s->id] = $s->ip.($s->comment ? ' [ '.$s->comment.' ]' : '');
		}
		$this->render('users/edit', array('model' => $model, 'servers' => $servers));
	}

}