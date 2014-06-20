<?php

class QuotasController extends Controller {
	public function filters() {
		return array(
			'accessControl',
			);
	}

	public function accessRules() {
		return array(
			array('allow',
				'actions'=>array('manage'),
				'users'=>array('@'),
				// access granted only for admins and operators
				'expression' => '(Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3)',
				),
			array('deny',
				'users'=>array('*'),
				),
			);
	}

	public function actionManage() {
		$id = Yii::app()->user->getId();
		$model = new QuotaForm;
		if(isset($_POST['ProfileForm'])) {
			$model->attributes = $_POST['ProfileForm'];
			if($model->validate() && $model->change()) {
				if($id == 'pass') {
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('user', 'Password successfully changed')));
				} else {
					Yii::app()->user->setFlash('notifyTO', array('type' => 'success', 'message' => Yii::t('user', 'Time zone successfully changed')));
				}
				$this->render('profile', array('model'=>$model));
				Yii::app()->end();
			}
		}
		$this->render('index', array('form' => $model, 'myCams' => Cams::model()->findAllByAttributes(array('user_id' => $id))));
	}
}