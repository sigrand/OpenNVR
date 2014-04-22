<?php

/**
 * ContactForm class.
 * ContactForm is the data structure for keeping
 * contact form data. It is used by the 'contact' action of 'SiteController'.
 */
class RegForm extends CFormModel {
	//public $atrs;
	public $nick;
	public $email;
	public $pass;
	//public $verifyCode;

	/**
	 * Declares the validation rules.
	 */
	public function rules()	{
		return array(
			// name, email, subject and body are required
			array('email, pass, nick', 'required'),
			array('email', 'unique', 'attributeName' => 'email', 'className' => 'Users', 'message' => Yii::t('errors', 'An account with this email address already exists')),
			array('nick', 'unique', 'attributeName' => 'nick', 'className' => 'Users', 'message' => Yii::t('errors', 'An account with this nick already exists')),
			array('email', 'email', 'message' => Yii::t('errors', 'Wrong email format')),
			//array('email, pass, nick', 'safe'),
			//array('verifyCode', 'captcha', 'allowEmpty'=>!CCaptcha::checkRequirements()),
			);
	}

	public function register() {
		$user = new Users;
		$user->attributes = $this->attributes;
		$salt = md5(uniqid().time());
		$user->email = $this->email;
		$user->salt = $salt;
		$user->pass = crypt(trim($this->pass).$salt);
		if($user->validate() && $user->save()) {
			if(!Settings::model()->getValue('mail_confirm')) {
				$user->status = 1;
				$user->save();
				return 1;
			}
			Yii::import('ext.YiiMailer.YiiMailer');
			$code = md5(md5($user->pass.$user->email));
			$mail = new YiiMailer();
			$mail->setFrom(Settings::model()->getValue('register'));
			$mail->setTo($user->email);
			$mail->setSubject(Yii::t('register', 'Account activation'));
			$mail->setBody(
				Yii::t(
					'register',
					"Hello {nick},<br/><br/>Your activation code: {code}<br/>{link}",
					array(
						'{nick}' => $user->nick,
						'{code}' => $code,
						'{link}' => Yii::app()->createAbsoluteUrl('site/confirm', array('user' => $user->nick, 'code' => $code)),
						)
					)
				);
				$mail->send();
				return 1;
			}
		}

		public function attributeLabels() {
			return array(
				'verifyCode' => Yii::t('register', 'Code:'),
				'pass' => Yii::t('register', 'Password'),
				'nick' => Yii::t('register', 'Nick')
				);
		}
	}