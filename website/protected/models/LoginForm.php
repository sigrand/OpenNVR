<?php
class LoginForm extends CFormModel {
	public $email;
	public $pass;
	public $rememberMe;
	private $_identity;

	public function rules()	{
		return array(
			array('email, pass', 'required', 'message' => 'Не можеть быть пустым!'),
			array('rememberMe', 'boolean'),
			array('pass', 'authenticate'),
			);
	}

	public function attributeLabels() {
		return array(
			'email' => 'Ваш email',
			'rememberMe' => 'Запомнить вас?',
			'pass' => 'Пароль',
			);
	}

	public function authenticate($attribute,$params) {
		//print_r(Users::model()->find('LOWER(email)=?', array($this->email)));
		if(!$this->hasErrors()) {
			$this->_identity = new UserIdentity($this->email, $this->pass);
			$authResult = $this->_identity->authenticate();
			if(!$authResult) {
				$this->addError('pass', 'Неправильный email или пароль.');
			} elseif($authResult === 2) {
				$this->addError('pass', 'Вы не активировали учетную запись, проверьте email.');
			} elseif($authResult === 3) {
				$this->addError('pass', 'Вас забанили.');
			}
		}
	}

	public function login() {
		if($this->_identity === null) {
			$this->_identity = new UserIdentity($this->email, $this->pass);
			$this->_identity->authenticate();
		}
		if($this->_identity->errorCode === UserIdentity::ERROR_NONE) {
			$duration = $this->rememberMe ? 3600*24*7 : 0; // 7 days
			Yii::app()->user->login($this->_identity, $duration);
			return true;
		} else {
			return false;
		}
	}
}
