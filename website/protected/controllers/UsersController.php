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

	public function actionNotifications($id = 0, $action = '') {
		if($id) {
			$notify = Notify::model()->findByPK($id);
			if(!$notify) {
				throw new CHttpException(404, Yii::t('errors', 'Wrong notify'));
			} elseif($action) {
				if($notify->status == 2 || $notify->status == 3) {
					throw new CHttpException(404, Yii::t('errors', 'Status already set'));
				}
				if($notify->shared_id) {
					$cam = Shared::model()->findByPK($notify->shared_id);
					if(!$cam) {
						throw new CHttpException(404, Yii::t('errors', 'Wrong cam'));
					}
				} else {
					throw new CHttpException(404, Yii::t('errors', 'Wrong cam'));
				}
				$n = new Notify;
				$id = Yii::app()->user->getId();
				if($action == 'approve') { //TODO specify user and cam
					$n->note(Yii::t('user', 'User decline shared cam'), array($id, $notify->creator_id, 0));
					$cam->is_approved = 1;
					$cam->save();
					$notify->status = 2;
					$notify->save();
				} elseif($action == 'disapprove') {
					$n->note(Yii::t('user', 'User decline shared cam'), array($id, $notify->creator_id, 0));
					$cam->is_approved = 0;
					$cam->save();
					$notify->status = 3;
					$notify->save();
				} else {
					throw new CHttpException(404, Yii::t('errors', 'Wrong action'));
				}
			} else {
				throw new CHttpException(404, Yii::t('errors', 'Wrong action'));
			}
		}
		$new = Notify::model()->findAllByAttributes(array('dest_id' => Yii::app()->user->getId(), 'status' => 1));
		$old = Notify::model()->findAllByAttributes(array('dest_id' => Yii::app()->user->getId(), 'status' => array(0, 2, 3)), array('order' => 'time DESC'));
		foreach ($new as $notify) {
			if($notify->shared_id == 0) {
				$notify->status = 0;
				$notify->save();
			}
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
					Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('user', 'Password successfully changed')));
				} else {
					Yii::app()->user->setFlash('notifyTO', array('type' => 'success', 'message' => Yii::t('user', 'Time zone successfully changed')));
				}
				$this->render('profile', array('model'=>$model));
				Yii::app()->end();
			}
		}
		$this->render('profile', array('model'=>$model));
	}
}