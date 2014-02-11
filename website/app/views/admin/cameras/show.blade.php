@extends('admin._layouts.default')

@section('main')
	<h2>Display camera</h2>

	<hr>

	<h3>{{ $camera->title }}</h3>
	<h5>@{{ $camera->created_at }}</h5>
	{{ $camera->body }}
@stop