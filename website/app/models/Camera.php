<?php namespace App\Models;

class Camera extends \Eloquent {

	protected $table = 'cameras';

	public function author()
	{
		return $this->belongsTo('App\Models\User', 'user_id');
	}
}

