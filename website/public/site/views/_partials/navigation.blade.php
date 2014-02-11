<nav id="nav">
	<ul>
		<li class="{{ (Route::is('home')) ? 'active' : null }}"><a href="{{ route('home') }}">Home</a></li>
		<li class="{{ (Route::is('camera') and Request::segment(1) == 'about-us') ? 'active' : null }}"><a href="{{ route('camera', 'about-us') }}">About us</a></li>
		<li class="{{ (Route::is('article.list') or Route::is('article')) ? 'active' : null }}"><a href="{{ route('article.list') }}">Blog</a></li>
		<li class="{{ (Route::is('camera') and Request::segment(1) == 'contact') ? 'active' : null }}"><a href="{{ route('camera', 'contact') }}">Contact</a></li>
	</ul>
</nav>
