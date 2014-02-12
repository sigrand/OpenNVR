<?php
class LoginForm extends CFormModel {
	public $email;
	public $pass;
	public $rememberMe;
	private $_identity;

	public function rules()	{
		return array(
			array('email, pass', 'required', 'message' => Yii::t('errors', 'Не можеть быть пустым!')),
			array('rememberMe', 'boolean'),
			array('pass', 'authenticate'),
			);
	}

	public function attributeLabels() {
		return array(
			'email' => Yii::t('register', 'Ваш email'),
			'rememberMe' => Yii::t('register', 'Запомнить вас?'),
			'pass' => Yii::t('register', 'Пароль'),
			);
	}

	public function authenticate($attribute,$params) {
		//print_r(Users::model()->find('LOWER(email)=?', array($this->email)));
		if(!$this->hasErrors()) {
			$this->_identity = new UserIdentity($this->email, $this->pass);
			$authResult = $this->_identity->authenticate();
			if(!$authResult) {
				$this->addError('pass', Yii::t('errors', 'Неправильный email или пароль.'));
			} elseif($authResult === 2) {
				$this->addError('pass', Yii::t('errors', 'Вы не активировали учетную запись, проверьте email.'));
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
