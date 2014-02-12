<?php

class UsersController extends Controller {
	public function filters() {
		return array(
			'accessControl',
			);
	}

	public function accessRules() {
		return array(
			array('allow',
				'actions'=>array('notifications', 'profile'),
				'users'=>array('@'),
				),
			array('deny',
				'users'=>array('*'),
				),
			);
	}

	public function actionNotifications() {
		$new = Notify::model()->findAllByAttributes(array('dest_id' => Yii::app()->user->getId(), 'is_new' => 1));
		$old = Notify::model()->findAllByAttributes(array('dest_id' => Yii::app()->user->getId(), 'is_new' => 0));
		foreach ($new as $notify) {
			$notify->is_new = 0;
			$notify->save();
		}
		$this->render('notify',
			array(
				'new' => $new,
				'old' => $old,
				)
			);
	}

	public function actionProfile($id) {
		$model = new ProfileForm;
		$model->setType($id);
		if(isset($_POST['ProfileForm'])) {
			$model->attributes = $_POST['ProfileForm'];
			if($model->validate() && $model->change()) {
				if($id == 'pass') {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('user', 'Пароль успешно изменен')));
			} else {
				Yii::app()->user->setFlash('notifyTO', array('type' => 'success', 'message' => Yii::t('user', 'Часовой пояс успешно изменен')));
			}
				$this->render('profile', array('model'=>$model));
				Yii::app()->end();
			}
		}
		$this->render('profile', array('model'=>$model));
	}
}