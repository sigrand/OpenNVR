<?php namespace App\Controllers\Admin;

use App\Models\Camera;
use App\Services\Validators\CameraValidator;
use Input, Notification, Redirect, Sentry, Str;

class CamerasController extends \BaseController {

	public function index()
	{
		return \View::make('admin.cameras.index')->with('cameras', Camera::all());
	}

	public function show($id)
	{
		return \View::make('admin.cameras.show')->with('camera', Camera::find($id));
	}

	public function create()
	{
		return \View::make('admin.cameras.create');
	}

	public function store()
	{
		$validation = new CameraValidator;

		if ($validation->passes())
		{
			$camera = new Camera;
			$camera->title       = Input::get('title');
            $camera->url         = Input::get('url');
            $camera->description = Input::get('description');
			$camera->user_id     = Sentry::getUser()->id;
			$camera->save();

			Notification::success('The camera was saved.');

			return Redirect::route('admin.cameras.edit', $camera->id);
		}

		return Redirect::back()->withInput()->withErrors($validation->errors);
	}

	public function edit($id)
	{
		return \View::make('admin.cameras.edit')->with('camera', Camera::find($id));
	}

    public function share($id)
    {
        return \View::make('admin.cameras.share')->with('camera', Camera::find($id));
    }

	public function update($id)
	{
		$validation = new CameraValidator;

		if ($validation->passes())
		{
			$camera = Camera::find($id);
            $camera->user_id     = Sentry::getUser()->id;
			$camera->title       = Input::get('title');
            $camera->description = Input::get('description');
            $camera->url         = Input::get('url');
			$camera->save();

			Notification::success('The camera was saved.');

			return Redirect::route('admin.cameras.edit', $camera->id);
		}

		return Redirect::back()->withInput()->withErrors($validation->errors);
	}

	public function destroy($id)
	{
		$camera = Camera::find($id);
		$camera->delete();

		Notification::success('The camera was deleted.');

		return Redirect::route('admin.cameras.index');
	}

}
