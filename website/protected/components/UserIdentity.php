<?php

class UserIdentity extends CUserIdentity {
	private $_id;
	public $isAdmin;
	public function authenticate() {
		$nick = strtolower($this->username);
		$user = Users::model()->find('LOWER(email)=?', array($nick));
		if($user === null) {
			$this->errorCode = self::ERROR_USERNAME_INVALID;
		} elseif(!$user->validatePassword($this->password, $user->salt)) {
			$this->errorCode = self::ERROR_PASSWORD_INVALID;
		} elseif($user->status == 0) {
			$this->errorCode = self::ERROR_PASSWORD_INVALID;
			return 2;
		} elseif($user->status == 2) {
			$this->errorCode = self::ERROR_PASSWORD_INVALID;
			return 3;
		} else {
			$this->_id = $user->id;
			$this->username = $user->email;
			$this->setState('isAdmin', $user->status == 3);
			$this->errorCode = self::ERROR_NONE;
		}
		return $this->errorCode == self::ERROR_NONE;
	}

	public function getId()	{
		return $this->_id;
	}
}