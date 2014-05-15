<?php

class Notify extends Notifications {
	public function rules()	{
		return parent::rules();
	}

	static public function note($msg, $opt = array(0, 0, 0), $is_assign = 1) {
		list($creator, $destination, $shared_id) = $opt;
		$note = new Notifications;
		$note->msg = $msg;
		$note->time = time();
		$note->creator_id = $creator;
		$note->dest_id = $destination;
		$note->shared_id = $shared_id;
		$note->status = $is_assign == 2 ? 2 : 1;
		return $note->validate() && $note->save();
	}

}