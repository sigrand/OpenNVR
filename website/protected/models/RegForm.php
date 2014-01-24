<?php

/**
 * ContactForm class.
 * ContactForm is the data structure for keeping
 * contact form data. It is used by the 'contact' action of 'SiteController'.
 */
class RegForm extends CFormModel
{
	//public $atrs;
	public $nick;
	public $email;
	public $pass;
	//public $verifyCode;

	/**
	 * Declares the validation rules.
	 */
	public function rules()
	{
		return array(
			// name, email, subject and body are required
			array('email, pass, nick', 'required', 'message' => 'Не может быть пустым'),
			array('email', 'unique', 'attributeName' => 'email', 'className' => 'Users', 'message' => 'На этот email, уже зарегестрирован пользователь'),
			array('nick', 'unique', 'attributeName' => 'nick', 'className' => 'Users', 'message' => 'Такой Ник, уже занят'),
			array('email', 'email', 'message' => 'Неправильный формат email'),
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
			Yii::import('ext.YiiMailer.YiiMailer');
			$code = md5(md5($user->pass.$user->email));
			$mail = new YiiMailer();
			$mail->setFrom('register@camshot.ru');
			$mail->setTo($user->email);
			$mail->setSubject('Активация аккаунта');
			$mail->setBody("\r\nЗдравствуйте $user->nick,<br/><br/>\r\n".
				"\r\n".
				'Ваш код активации:<br/> '.
				$code.
				"\r\n".
				'<br/>Ссылка для активации:<br/> '.
				Yii::app()->createAbsoluteUrl('site/confirm', array('user' => $user->nick, 'code' => $code))
			);
			$mail->send();
			//print_r($mail->getError());
			///die();
			return 1;
		}
	}

	public function attributeLabels()
	{
		return array(
			'verifyCode'=>'Код:',
			'pass'=>'Пароль',
			'nick'=>'Ник'
			);
	}
}