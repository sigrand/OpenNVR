@extends('admin._layouts.default')

@section('main')

	<h1>
		Cameras <a href="{{ URL::route('admin.cameras.create') }}" class="btn btn-success"><i class="icon-plus-sign"></i> Add new camera</a>
	</h1>

	<hr>

	{{ Notification::showAll() }}

	<table class="table table-striped">
		<thead>
			<tr>
				<th>URL</th>
                <th>Title</th>
                <th>Description</th>
				<th>When</th>
				<th><i class="icon-cog"></i></th>
			</tr>
		</thead>
		<tbody>
			@foreach ($cameras as $camera)
				<tr>
                    <td>{{ $camera->url }}</td>
					<td><a href="{{ URL::route('admin.cameras.show', $camera->id) }}">{{ $camera->title }}</a></td>
                    <td>{{ $camera->description }}</td>
					<td>{{ $camera->created_at }}</td>
					<td>
                        <a href="http://url-to-share-camera" class="btn btn-success btn-mini pull-left">Share</a>
						<a href="{{ URL::route('admin.cameras.edit',  $camera->id) }}" class="btn btn-success btn-mini pull-left">Edit</a>

						{{ Form::open(array('route' => array('admin.cameras.destroy', $camera->id), 'method' => 'delete', 'data-confirm' => 'Are you sure?')) }}
							<button type="submit" href="{{ URL::route('admin.cameras.destroy', $camera->id) }}" class="btn btn-danger btn-mini">Delete</button>
						{{ Form::close() }}
					</td>
				</tr>
			@endforeach
		</tbody>
	</table>

@stop
