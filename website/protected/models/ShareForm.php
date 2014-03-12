<?php

class ShareForm extends CFormModel {
	private $camBuff;
	private $emailBuff;
	public $hcams;
	public $cams;
	public $emails;

	public function rules()	{
		return array(
			array('cams, emails, hcams', 'required', 'message' => Yii::t('errors', 'Can\'t be empty')),
			array('hcams', 'isOwned'),
			);
	}

	public function beforeValidate() {
		if(parent::beforeValidate()) {
			$cams = array_map('trim', explode(',', $this->hcams));
			$c = count($cams);
			$this->camBuff = Cams::model()->findAllByAttributes(array('id' => $cams));
			if(empty($this->camBuff) || count($this->camBuff) != $c) {
				$this->addError('cams', $Yii::t('errors', 'There is no such cam'));
				return false;
			}
			$emails = array_map('trim', explode(',', $this->emails));
			$c = count($emails);
			$this->emailBuff = Users::model()->findAllByAttributes(array('email' => $emails));
			if(empty($this->emailBuff) || count($this->emailBuff) != $c) {
				$this->addError('emails', $Yii::t('errors', 'There is no such user'));
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
				if($cam->user_id != Yii::app()->user->getId()) { $this->addError('cams', $c > 1 ? Yii::t('errors', 'You are not owner of this cameras') : Yii::t('errors', 'You are not owner of this cameras')); return false; }
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
				$n->note(Yii::t('cams', 'You granted access to camera').' '.$cam->name, $id, $user->id);
				$Shared = Shared::model()->findByAttributes(array('owner_id' => $id, 'user_id' => $user->id, 'cam_id' => $cam->id, 'public' => 0));
				if(!$Shared) {
					$Shared = new Shared;
					$Shared->owner_id = $id;
					$Shared->user_id = $user->id;
					$Shared->cam_id = $cam->id;
					$Shared->save();
				}
			}
		}
		return true;
	}

	public function attributeLabels() {
		return array(
			'hcams'  => Yii::t('Cams, can be list of cams separated ","'),
			'cams'   => Yii::t('Cams, can be list of cams separated ","'),
			'emails' => Yii::t('Emails/groups, can be list separated ","')
			);
	}

}