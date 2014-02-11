<?php

// Home page
Route::get('/', array('as' => 'home', function()
{
	return View::make('site::index')->with('entry', Camera::where('slug', 'welcome')->first());
}));

// Article list
Route::get('blog', array('as' => 'article.list', function()
{
	return View::make('site::articles')->with('entries', Article::orderBy('created_at', 'desc')->get());
}));

// Single article
Route::get('blog/{slug}', array('as' => 'article', function($slug)
{
	$article = Article::where('slug', $slug)->first();

	if ( ! $article) App::abort(404, 'Article not found');

	return View::make('site::article')->with('entry', $article);
}));

// Single page
Route::get('{slug}', array('as' => 'camera', function($slug)
{
	$camera = Camera::where('slug', $slug)->first();

	if (!$camera) App::abort(404, 'Camera not found');

	return View::make('site::camera')->with('entry', $camera);

}))->where('slug', '^((?!admin).)*$');

// 404 Page
App::missing(function($exception)
{
	return Response::view('site::404', array(), 404);
});


