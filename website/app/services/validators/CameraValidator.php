<?php namespace App\Services\Validators;

class CameraValidator extends Validator {

	public static $rules = array(
		'title' => 'required',
		'url'  => 'required',
	);

}
