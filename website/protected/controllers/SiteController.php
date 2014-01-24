<?php

class SiteController extends Controller {

	public function actionIndex() {
		$this->render('index');
	}

	public function actionAbout() {
		Notify::note('Кто то посетил about');
		$this->render('info', array('title' => 'О нас', 'content' => 'О нас...'));
	}

	public function actionRegister() {
		$model = new RegForm;
		if(isset($_POST['ajax']) && $_POST['ajax']==='reg-form') {
			echo CActiveForm::validate($model);
			Yii::app()->end();
		}
		if(isset($_POST['RegForm'])) {
			$model->attributes = $_POST['RegForm'];
			if($model->validate() && $model->register()) {
				$this->render('confirm');
				Yii::app()->end();
			}
		}
		$this->render('register', array('model'=>$model));
	}

	public function actionConfirm() {
		if(!isset($_GET['code'])) {
			$this->render('confirm');
		} else {
			$user = Users::model()->findByAttributes(array('nick' => $_GET['user']));
			if(!$user) {
				$code = 2;
			} elseif($user->status == 1) {
				$code = 1;
			} else {
				$userCode = md5(md5($user->pass.$user->email));
				if($_GET['code'] != $userCode) {
					$code = 3;
				} else {
					$user->status = 1;
					$user->save();
					$code = 4;
				} 
			}
			$this->render('confirm', array('activation' => $code));
		}
	}

	public function actionRecovery() {
		if(isset($_POST['pass']) && isset($_POST['pass2'])) {
			if(empty($_POST['pass']) || empty($_POST['pass2'])) {
				$code = 5;
			} elseif($_POST['pass'] !== $_POST['pass2']) {
				$code = 4;
			} else {
				$user = Users::model()->findByAttributes(array('nick' => $_GET['user']));
				if(!$user) {
					$code = 1;
				} else {
					$userCode = md5(md5($user->pass.$user->email));
					if($_GET['code'] != $userCode) {
						$code = 2;
					} else {
						$code = 6;
						$user->pass = crypt($_POST['pass'].$user->salt);
						$user->save();
					}
				}
			}
			$this->render('recovery', array('code' => $code));
		} elseif(isset($_GET['user']) && isset($_GET['code'])) {
			$user = Users::model()->findByAttributes(array('nick' => $_GET['user']));
			if(!$user) {
				$code = 1;
			} else {
				$userCode = md5(md5($user->pass.$user->email));
				if($_GET['code'] != $userCode) {
					$code = 2;
				} else {
					$code = 3;
				}
			}
			$this->render('recovery', array('code' => $code));
		} elseif(!isset($_POST['nick']) || !isset($_POST['email'])) {
			$this->render('recovery', array('result' => ''));
		} else {
			$answer = 'Нет такого пользователя';
			if(isset($_POST['nick']) && !empty($_POST['nick'])) {
				$user = Users::model()->findByAttributes(array('nick' => $_POST['nick']));
				if(!$user) {
					$answer = 'Нет такого пользователя';
				} else {
					$mail = 1;
					$answer = 'Письмо с данными для восстановления отправлено на ваш email';
				}
			} elseif(isset($_POST['email']) && !empty($_POST['email'])) {
				$user = Users::model()->findByAttributes(array('email' => $_POST['email']));
				if(!$user) {
					$answer = 'Нет такого пользователя';
				} else {
					$mail = 1;
					$answer = 'Письмо с данными для восстановления отправлено на ваш email';
				}
			}
			if(isset($mail) && $mail) {
				Yii::import('ext.YiiMailer.YiiMailer');
				$code = md5(md5($user->pass.$user->email));
				$mail = new YiiMailer();
				$mail->setFrom('recovery@camshot.ru');
				$mail->setTo($user->email);
				$mail->setSubject('Восстановление пароля');
				$mail->setBody(
					"\r\n".
					'Ссылка для восстановления: '.
					Yii::app()->createAbsoluteUrl('site/recovery', array('user' => $user->nick, 'code' => $code))
					);
				$mail->send();
			}
			$this->render('recovery', array('result' => $answer));
		}
	}

	public function actionError() {
		if($error=Yii::app()->errorHandler->error) {
			if(Yii::app()->request->isAjaxRequest)
				echo $error['message'];
		} else {
			$this->render('error', $error);
		}
	}

	public function actionLogin() {
		$model=new LoginForm;
		if(isset($_POST['ajax']) && $_POST['ajax']==='login-form') {
			echo CActiveForm::validate($model);
			Yii::app()->end();
		}
		if(isset($_POST['LoginForm'])) {
			$model->attributes=$_POST['LoginForm'];
			if($model->validate() && $model->login())
				$this->redirect($this->createUrl('cams/index'));
		}
		$this->render('login',array('model'=>$model));
	}

	public function actionLogout() {
		Yii::app()->user->logout();
		$this->redirect(Yii::app()->homeUrl);
	}
}