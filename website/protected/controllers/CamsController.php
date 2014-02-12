<?php

class CamsController extends Controller {

	private $buff = array();
	public $showArchive = true;
	public $showStatusbar = true;

	public function __construct($id,$module=null) {
		parent::__construct($id,$module);
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
				'actions'=>array('index', 'fullscreen', 'playlist'),
				'users'=>array('*'),
				),
			array('allow',
				'actions'=>array('add', 'edit', 'manage', 'delete', 'share', 'fullscreen', 'existence', 'unixtime', 'map'),
				'users'=>array('@'),
				// разрешаем достут только операторам и админам
				'expression' => '(Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3)',
				),
			array('deny',
				'users'=>array('*'),
				),
			);
	}

	public function actionUnixtime() {
		$this->layout = 'emptycolumn';
		$momentManager = new momentManager;
		$this->render('empty',
			array(
				'content' => $momentManager->unixtime()
				)
			);
	}

	public function actionExistence($stream) {
		$this->layout = 'emptycolumn';
		$momentManager = new momentManager;
		$this->render('empty',
			array(
				'content' => $momentManager->existence($stream)
				)
			);
	}

	public function actionPlaylist() {
		$this->layout = 'emptycolumn';
		$momentManager = new momentManager;
		$this->render('empty',
			array(
				'content' => $momentManager->playlist()
				)
			);
	}

	public function actionIndex() {
		$id = Yii::app()->user->getId();
		$publicAll = Cams::model()->findAllByAttributes(array('public' => 1));
		$shared = Shared::model()->findAllByAttributes(array('user_id' => $id, 'public' => 0, 'show' => 1));
		$publicEdited = Shared::model()->findAllByAttributes(array('user_id' => $id, 'public' => 1), array('index' => 'cam_id'));
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
		$this->render('index3',
			array(
				'myCams' => Cams::model()->findAllByAttributes(array('user_id' => $id, 'show' => 1)),
				'mySharedCams' => $shared,
				'myPublicCams' => $public,
				)
			);
	}

	public function actionFullscreen($id, $preview = 0, $full = 0) {
		if(!is_numeric($id)) { $this->redirect('/player/'.$id); }
		//$this->layout = 'fullscreencolumn';
		$this->layout = 'emptycolumn';
		$cam = Cams::model()->findByPK($id);
		if(!$cam) {
			throw new CHttpException(404, Yii::t('errors', 'Нет такой камеры'));
		}
		if(!$cam->public && Yii::app()->user->isGuest) {
			throw new CHttpException(403, Yii::t('errors', 'Доступ запрещен'));
		}
		$uid = Yii::app()->user->getId();
		$this->showArchive = !$preview && (!$cam->public || $uid == $cam->user_id) ? 1 : 0;
		$this->showStatusbar = !$preview;
		if($cam->user_id != $uid) {
			$shared = (bool)Shared::model()->findByAttributes(array('user_id' => $uid, 'cam_id' => $id));
			if(!$cam->public && !$shared) {
				throw new CHttpException(403, Yii::t('errors', 'Доступ запрещен'));
			}
		}
		$this->showStatusbar = $this->showArchive = !$preview && ((!$cam->public || $shared) || $uid == $cam->user_id) ? 1 : 0;
		$this->render('fullscreen3', array('cam' => $cam, 'low' => !$full && isset($cam->prev_url) && !empty($cam->prev_url)));
	}

	public function actionAdd() {
		$model = new Cams;
		if(isset($_POST['Cams'])) {
			$model->attributes = $_POST['Cams'];
			$model->user_id = Yii::app()->user->getId();
			if($model->validate() && $model->save()) {
				$momentManager = new momentManager;
				if($momentManager->add($model)) {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камера успешно добавлена')));
				} else {
					Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('cams', 'Камера не добавлена, проблемы с Moment`ом')));
					$model->delete();
				}
				$this->redirect(array('manage'));
			}
		}
		$this->render('edit', array('model' => $model));
	}

	public function actionEdit($id) { // TODO add check owner
		$model = Cams::model()->findByPK($id);
		if(!$model) {
			$this->redirect(array('manage'));
		}
		if(isset($_POST['Cams'])) {
			$model->attributes = $_POST['Cams'];
			$model->user_id = Yii::app()->user->getId();
			if($model->validate()) {
				$momentManager = new momentManager;
				if($momentManager->edit($model) && $model->save()) {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камера успешно изменена')));
				} else {
					Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('cams', 'Камера не изменена, проблемы с Moment`ом')));
				}
				$this->redirect(array('edit', 'id' => $model->id));
			}
		}
		$this->render('edit', array('model' => $model));
	}

	public function actionDelete($id, $type = 'cam') { // TODO add check owner
		if($type == 'share') {
			$model = Shared::model()->findByPK($id);
		} else {
			$model = Cams::model()->findByPK($id);
		}
		if(!$model) {
			$this->redirect(array('manage'));
		}

		$momentManager = new momentManager;

		if($type != 'share' && $model->delete()) {
			$momentManager->delete($model);
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камера успешно удалена')));
		} elseif ($type == 'share' && $model->delete()) {
			Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камера успешно удалена')));
		} else {
			Yii::app()->user->setFlash('notify', array('type' => 'danger', 'message' => Yii::t('cams', 'Камера не удалена, проблемы с Moment`ом')));
		}
		
		$this->redirect(array('manage'));
	}

	public function actionShare() { // TODO add check owner
		$form = new ShareForm;
		if(isset($_POST['ShareForm'])) {
			$form->attributes = $_POST['ShareForm'];
			if($form->validate() && $form->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камеры успешно расшарены')));
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
		$momentManager = new momentManager;
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
					if($key[0] == 'shcam') {
						$cam = Shared::model()->findByPK((int)$key[1]);
					} elseif($key[0] == 'pcam') {
						$cam = Shared::model()->findByAttributes(array('cam_id' => (int)$key[1], 'user_id' => $id, 'public' => '1'));
						if(!$cam) {
							$cam = new Shared;
							$cam->owner_id = Cams::model()->findByPK((int)$key[1])->user_id;
							$cam->user_id = $id;
							$cam->cam_id = (int)$key[1];
							$cam->show = 1;
							$cam->public = 1;
						}
					} else {
						$cam = Cams::model()->findByPK((int)$key[1]);
					}
					if(!$cam) { continue; }
					if(isset($_POST['show'])) {
						$cam->show = $cam->show ? 0 : 1;
						if($cam->save()) {
							Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Настройки камер изменены')));
						}
					} elseif(isset($_POST['del'])) {
						if($key[0] == 'shcam') {
							if($cam->delete()) {
								Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камеры удалены')));
							}
						} else {
							if($cam->delete()) {
								$momentManager->delete($cam);
								Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Камеры удалены')));
							}
						}
					} elseif(isset($_POST['record'])) {
						$cam->record = $cam->record ? 0 : 1;
						$momentManager->rec($cam->record == 1 ? 'on' : 'off', $cam->id);
						if($cam->save()) {
							Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('cams', 'Настройки камер изменены')));
						}
					}
				}
			}
			$this->refresh();
			Yii::app()->end();
		} elseif(isset($_POST['show']) || isset($_POST['del']) || isset($_POST['share'])) {
			Yii::app()->user->setFlash('notify', array('type' => 'warning', 'message' => Yii::t('cams', 'Для начала выберите камеры')));
		}
		$public = array();
		$publicAll = Cams::model()->findAllByAttributes(array('public' => 1));
		$shared = Shared::model()->findAllByAttributes(array('user_id' => $id, 'public' => 0));
		foreach ($shared as $key => $value) {
			if(!isset($value->cam->id)) {
				$value->delete();
				unset($shared[$key]);
			}
		}
		$publicEdited = Shared::model()->findAllByAttributes(array('user_id' => $id, 'public' => 1), array('index' => 'cam_id'));
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
		$camName = Cams::model()->findByPK((int)$camName[1]);
		$this->buff[$cam] = $camName->id; 
		return $camName->name;
	}

	private function compileHCams($cam) {
		if(isset($this->buff[$cam])) {
			return $this->buff[$cam];
		}
	}
}