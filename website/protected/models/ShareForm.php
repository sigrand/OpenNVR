<?php

class ShareForm extends CFormModel {
	private $camBuff;
	private $emailBuff;
	public $hcams;
	public $cams;
	public $emails;

	public function rules()	{
		return array(
			array('cams, emails, hcams', 'required'),
			array('hcams', 'isOwned'),
			);
	}

	public function beforeValidate() {
		if(parent::beforeValidate()) {
			$cams = array_map('trim', explode(',', $this->hcams));
			$c = count($cams);
			foreach ($cams as $cam) {
				$this->camBuff[] = Cams::model()->findByPK(Cams::model()->getRealId($cam));
			}
			$this->camBuff = array_filter($this->camBuff);
			if(empty($this->camBuff) || count($this->camBuff) != $c) {
				$this->addError('cams', $c > 1 ? Yii::t('errors', 'One of cam is wrong') : Yii::t('errors', 'There is no such cam'));
				return false;
			}
			$emails = array_map('trim', explode(',', $this->emails));
			$c = count($emails);
			$this->emailBuff = Users::model()->findAllByAttributes(array('email' => $emails));
			if(empty($this->emailBuff) || count($this->emailBuff) != $c) {
				$this->addError('emails', $c > 1 ? Yii::t('errors', 'One of user is wrong') : Yii::t('errors', 'There is no such user'));
				return false;	
			}
			return true;
		}
		return false;
	}

	public function isOwned() {
		if($this->camBuff) {
			$c = count($this->camBuff);
			foreach ($this->camBuff as $cam) {
				if($cam->user_id != Yii::app()->user->getId()) { $this->addError('cams', $c > 1 ? Yii::t('errors', 'You are not owner of this cam') : Yii::t('errors', 'You are not owner of this cam')); return false; }
			}
			return true;
		}
		return false;
	}

	public function save() {
		$id = Yii::app()->user->getId();
		foreach($this->camBuff as $cam) {
			foreach($this->emailBuff as $user) {
				$n = new Notify;
				$shared = Shared::model()->findByAttributes(array('owner_id' => $id, 'user_id' => $user->id, 'cam_id' => $cam->id, 'is_public' => 0));
				if(!$shared) {
					$shared = new Shared;
					$shared->owner_id = $id;
					$shared->user_id = $user->id;
					$shared->cam_id = $cam->id;
					$shared->save();
				}
				$n->note(Yii::t('cams', 'You granted access to camera {cam}', array('{cam}' => $cam->name)), array($id, $user->id, $shared->id));
			}
		}
		return true;
	}

	public function attributeLabels() {
		return array(
			'hcams'  => Yii::t('cams', 'Cams, can be list of cams separated ","'),
			'cams'   => Yii::t('cams', 'Cams, can be list of cams separated ","'),
			'emails' => Yii::t('cams', 'Emails/groups, can be list separated ","')
			);
	}

}