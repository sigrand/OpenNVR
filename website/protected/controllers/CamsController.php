<?php

class CamsController extends Controller {

	private $buff = array();
	public $showArchive = true;
	public $showStatusbar = true;

	public function __construct($id,$module=null) {
		parent::__construct($id, $module);
		Yii::import('ext.moment.index', 1);
	}	

	public function filters() {
		return array(
			'accessControl',
			);
	}

	public function accessRules() {
		return array(
			array('allow',
				'actions'=>array('index', 'fullscreen', 'playlist', 'check', 'map', 'list'),
				'users'=>array('*'),
				),
			array('allow',
				'actions'=>array('add', 'edit', 'manage', 'delete', 'share', 'fullscreen', 'existence', 'unixtime', 'map', 'list'),
				'users'=>array('@'),
				// access granted only for admins and operators
				'expression' => '(Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3)',
				),
			array('deny',
				'users'=>array('*'),
				),
			);
	}

	public function actionUnixtime($id) {
		$this->layout = 'emptycolumn';
		$momentManager = new momentManager($id);
		$this->render('empty',
			array(
				'content' => $momentManager->unixtime()
				)
			);
	}

	public function actionCheck($id = '', $clientAddr = '') {
		if((!$id && !$clientAddr) && (isset($_POST['data']))) {
			$data = json_decode($_POST['data'], 1);
			$id = $data['id'];
			$clientAddr = $data['clientAddr'];
		}
		$response = array('result' => 'fail', 'id' => '');
		if(empty($id) && $this->validateRequest('id', $id)) {
			$response['id'] = 'empty or invalid id';
			$this->renderJSON($response);
		}
		if(empty($clientAddr) && $this->validateRequest('ip', $clientAddr)) {
			$response['id'] = 'empty or invalid ip';
			$this->renderJSON($response);
		}
		$session_id = Sessions::model()->findByAttributes(array('session_id' => $id), array('select' => 'real_id, ip'));
		if($session_id) {
			$cam = Cams::model()->findByPK($session_id->real_id);
			if($cam->is_public) {
				$response['result'] = 'success';
				$response['id'] = $session_id->real_id;
			} elseif($id->ip == $clientAddr) {
				$response['result'] = 'success';
				$response['id'] = $session_id->real_id;
			} else {
				$response['id'] = 'huge fail';
			}
		} else {
			$cam = Cams::model()->findByPK($id);
			if ($cam && $cam->is_public) {
				$response['result'] = 'success';
				$response['id'] = $id;
			} else {
				$response['id'] = 'huge fail';
			}
		}
		$this->renderJSON($response);
	}

	private function validateRequest($type, $value) {
		if($type == 'id') {
			return preg_match('/^[0-9a-f]{32}$/i', $value);
		} else {
			return filter_var($value, FILTER_VALIDATE_IP);
		}
		return true;
	}

	public function actionExistence($stream, $id) {
		$this->layout = 'emptycolumn';
		$momentManager = new momentManager($id);
		$this->render('empty',
			array(
				'content' => $momentManager->existence($stream)
				)
			);
	}

	public function actionPlaylist($id, $cam_id) {
		$this->layout = 'emptycolumn';
		$momentManager = new momentManager($id);
		$this->render('empty',
			array(
				'content' => $momentManager->playlist(Cams::model()->getRealId($cam_id), $cam_id)
				)
			);
	}

	public function actionIndex() {
		$this->redirect($this->createUrl('cams/'.Settings::model()->getValue('index')));
	}

	public function actionList() {
		$id = Yii::app()->user->getId();
		$publicAll = Cams::model()->findAllByAttributes(array('is_public' => 1));
		$shared = Shared::model()->findAllByAttributes(array('user_id' => $id, 'is_public' => 0, 'show' => 1));
		$publicEdited = Shared::model()->findAllByAttributes(array('user_id' => $id, 'is_public' => 1), array('index' => 'cam_id'));
		$public = array();
		foreach ($publicAll as $cam) {
			if(isset($publicEdited[$cam->id])) {
				if($publicEdited[$cam->id]->show) {
					$public[] = $publicEdited[$cam->id];
				}
			} else {
				$public[] = $cam;
			}
		}
		$this->render('index',
			array(
				'myCams' => Cams::model()->findAllByAttributes(array('user_id' => $id, 'show' => 1)),
				'mySharedCams' => $shared,
				'myPublicCams' => $public,
				)
			);
	}

	public function actionFullscreen($id, $preview = 0, $full = 0) {
		//if(!is_numeric($id)) { $this->redirect('/player/'.$id); }
		$this->layout = 'emptycolumn';
		$id = Cams::model()->getRealId($id);
		$cam = Cams::model()->findByPK($id);
		if(!$cam) {
			throw new CHttpException(404, Yii::t('errors', 'There is no such cam'));
		}
		if(!$cam->is_public && Yii::app()->user->isGuest) {
			throw new CHttpException(403, Yii::t('errors', 'Access denied'));
		}
		$uid = Yii::app()->user->getId();
		$shared = false;
		if($cam->user_id != $uid) {
			$shared = Shared::model()->findByAttributes(array('user_id' => $uid, 'cam_id' => $id));
			if(!$cam->is_public && !(bool)$shared) {
				throw new CHttpException(403, Yii::t('errors', 'Access denied'));
			}
		}
		$this->showStatusbar = $this->showArchive = !$preview && ((!$cam->is_public || ($shared && (!$shared->is_public || $shared->is_approved))) || $uid == $cam->user_id) ? 1 : 0;
		$server = Servers::model()->findByPK($cam->server_id);
		$low = !$full && isset($cam->prev_url) && !empty($cam->prev_url);
		$this->render('fullscreen',
			array(
				'cam' => $cam,
				'session_id' => $cam->getSessionId($low),
				'low' => $low,
				'down' => $server->protocol.'://'.$server->ip.':'.$server->w_port.'/mod_nvr/file?stream='
				));
	}

	public function actionAdd() {
		$model = new Cams;
		if(isset($_POST['Cams'])) {
			$model->attributes = $_POST['Cams'];
			$model->user_id = Yii::app()->user->getId();
			if($model->validate() && $model->save()) {
				$momentManager = new momentManager($model->server_id);
				if($momentManager->add($model)) {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cam successfully added')));
				} else {
					Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('cams', 'Cam not added. Problem with nvr')));
					$model->delete();
				}
				$this->redirect(array('manage'));
			}
		}
		$servers = array();
		$server = Servers::model()->findAll(array('select' => 'id, ip, comment'));
		foreach($server as $s) {
			$servers[$s->id] = $s->ip.($s->comment ? ' [ '.$s->comment.' ]' : '');
		}
		$this->render('edit', array('model' => $model, 'servers' => $servers));
	}

	public function actionEdit($id) { // TODO add check owner
		$model = Cams::model()->findByPK(Cams::model()->getRealId($id));
		if(!$model) {
			$this->redirect(array('manage'));
		}
		if(isset($_POST['Cams'])) {
			$model->attributes = $_POST['Cams'];
			$model->id = Cams::model()->getRealId($id);
			$model->user_id = Yii::app()->user->getId();
			if($model->validate()) {
				$momentManager = new momentManager($model->server_id);
				if($momentManager->edit($model) && $model->save()) {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cam successfully changed')));
				} else {
					Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('cams', 'Cam not changed. Problem with nvr')));
				}
				$this->redirect(array('edit', 'id' => $model->id));
			}
		}
		$servers = array();
		$server = Servers::model()->findAll(array('select' => 'id, ip, comment'));
		foreach($server as $s) {
			$servers[$s->id] = $s->ip.($s->comment ? ' [ '.$s->comment.' ]' : '');
		}
		$this->render('edit', array('model' => $model, 'servers' => $servers));
	}

	public function actionDelete($id, $type = 'cam') { // TODO add check owner
		if($type == 'share') {
			$model = Shared::model()->findByPK($id);
			$momentManager = new momentManager($model->cam->server_id);
		} else {
			$model = Cams::model()->findByPK(Cams::model()->getRealId($id));
			$momentManager = new momentManager($model->server_id);
		}
		if(!$model) {
			$this->redirect(array('manage'));
		}
		if($type != 'share' && $model->delete()) {
			$momentManager->delete($model);
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cams successfully deleted')));
		} elseif ($type == 'share' && $model->delete()) {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cams successfully deleted')));
		} else {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('cams', 'Cam not deleted. Problem with nvr')));
		}
		
		$this->redirect(array('manage'));
	}

	public function actionShare() { // TODO add check owner
		$form = new ShareForm;
		if(isset($_POST['ShareForm'])) {
			$form->attributes = $_POST['ShareForm'];
			if($form->validate() && $form->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cam successfully shared')));
				$this->redirect(array('manage'));
				Yii::app()->end();
			}
		}
		if(Yii::app()->user->hasFlash('share')) {
			$cams = Yii::app()->user->getFlash('share');
			$cams = array_keys(array_filter($cams));
			$form->cams = join(', ', array_map(array($this, 'compileCams'), $cams));
			$form->hcams = join(',', array_map(array($this, 'compileHCams'), $cams));
		}
		$this->render('share', array('model' => $form));
	}

	public function actionManage() { // TODO add check owner
		$id = Yii::app()->user->getId();
		if(isset($_POST['CamsForm']) && !empty($_POST['CamsForm']) && array_sum($_POST['CamsForm']) != 0) {
			if(isset($_POST['share'])) {
				Yii::app()->user->setFlash('share', $_POST['CamsForm']);
				$this->redirect(array('share'));
				Yii::app()->end();
			}
			foreach ($_POST['CamsForm'] as $key => $cam) {
				if($cam) {
					$key = explode('_', $key);
					$key[2] = Cams::model()->getRealId($key[1]);
					if($key[0] == 'shcam') {
						$cam = Shared::model()->findByPK((int)$key[1]);
					} elseif($key[0] == 'pcam') {
						$cam = Shared::model()->findByAttributes(array('cam_id' => $key[2], 'user_id' => $id, 'is_public' => '1'));
						if(!$cam) {
							$cam = new Shared;
							$cam->owner_id = Cams::model()->findByPK($key[2])->user_id;
							$cam->user_id = $id;
							$cam->cam_id = $key[2];
							$cam->show = 1;
							$cam->is_public = 1;
						}
					} else {
						$cam = Cams::model()->findByPK($key[2]);
					}
					if(!$cam) { continue; }
					if(isset($_POST['show'])) {
						$cam->show = $cam->show ? 0 : 1;
						if($cam->save()) {
							Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cams settings changed')));
						}
					} elseif(isset($_POST['del'])) {
						if($key[0] == 'shcam') {
							if($cam->delete()) {
								Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cams successfully deleted')));
							}
						} else {
							$momentManager = new momentManager($cam->server_id);
							if($cam->delete()) {
								$momentManager->delete($cam);
								Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cams successfully deleted')));
							}
						}
					} elseif(isset($_POST['record'])) {
						$cam->record = $cam->record ? 0 : 1;
						$momentManager = new momentManager($cam->server_id);
						$momentManager->rec($cam->record == 1 ? 'on' : 'off', $cam->id);
						if($cam->save()) {
							Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Cams settings changed')));
						}
					}
				}
			}
			$this->refresh();
			Yii::app()->end();
		} elseif(isset($_POST['show']) || isset($_POST['del']) || isset($_POST['share'])) {
			Yii::app()->user->setFlash('notify', array('type' => 'warning', 'message' => Yii::t('cams', 'You must choose cams to modify')));
		}
		$public = array();
		$publicAll = Cams::model()->findAllByAttributes(array('is_public' => 1));
		$shared = Shared::model()->findAllByAttributes(array('user_id' => $id, 'is_public' => 0, 'is_approved' => 1));
		foreach ($shared as $key => $value) {
			if(!isset($value->cam->id)) {
				$value->delete();
				unset($shared[$key]);
			}
		}
		$publicEdited = Shared::model()->findAllByAttributes(array('user_id' => $id, 'is_public' => 1), array('index' => 'cam_id'));
		foreach ($publicAll as $cam) {
			if(isset($publicEdited[$cam->id])) {
				$public[] = $publicEdited[$cam->id];
			} else {
				$public[] = $cam;
			}
		}
		$this->render('list',
			array(
				'form' => new CamsForm,
				'myCams' => Cams::model()->findAllByAttributes(array('user_id' => $id)),
				'mySharedCams' => $shared,
				'myPublicCams' => $public
				)
			);
	}

	public function actionMap() {
		$myCams = Cams::getAvailableCams();
		$this->render('map',
			array(
				'myCams' => $myCams,
				)
			);
	}

	private function compileCams($cam) { 
		$camName = explode('_', $cam);
		$camName = Cams::model()->findByPK(Cams::model()->getRealId($camName[1]));
		if(!$camName) {
			throw new CHttpException(404, Yii::t('errors', 'There is no such cam'));
		}
		$this->buff[$cam] = $camName->getSessionId(); 
		return $camName->name;
	}

	private function compileHCams($cam) {
		if(isset($this->buff[$cam])) {
			return $this->buff[$cam];
		}
	}
}