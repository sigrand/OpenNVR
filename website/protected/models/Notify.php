<?php

class Notify extends Notifications {
	public function rules()	{
		return parent::rules();
	}

	static public function note($msg, $creator = 0, $destination = 0) {
		$note = new Notifications;
		$note->msg = $msg;
		$note->time = time();
		$note->creator_id = $creator;
		$note->dest_id = $destination;
		return $note->validate() && $note->save();
	}

}