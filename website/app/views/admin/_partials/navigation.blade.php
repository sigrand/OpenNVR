@if (Sentry::check())
	<ul class="nav">
		<li class="{{ Request::is('admin/cameras*') ? 'active' : null }}"><a href="{{ URL::route('admin.cameras.index') }}"><i class="icon-book"></i> Cameras</a></li>
		<!-- <li class="{{ Request::is('admin/articles*') ? 'active' : null }}"><a href="{{ URL::route('admin.articles.index') }}"><i class="icon-edit"></i> Articles</a></li> -->
		<li><a href="{{ URL::route('admin.logout') }}"><i class="icon-lock"></i> Logout</a></li>
	</ul>
@endif
