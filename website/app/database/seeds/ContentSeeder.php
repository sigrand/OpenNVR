<?php

use App\Models\Article;
use App\Models\Camera;

class ContentSeeder extends Seeder {

	public function run()
	{
		DB::table('articles')->truncate(); // Using truncate function so all info will be cleared when re-seeding.
		DB::table('cameras')->truncate();

		Article::create(array(
			'title'   => 'First post',
			'slug'    => 'first-post',
			'body'    => 'Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.',
			'user_id' => 1,
			'created_at' => Carbon\Carbon::now()->subWeek(),
			'updated_at' => Carbon\Carbon::now()->subWeek(),
		));

		Article::create(array(
			'title'   => '2nd post',
			'slug'    => '2nd-post',
			'body'    => 'Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.',
			'user_id' => 1,
			'created_at' => Carbon\Carbon::now()->subDay(),
			'updated_at' => Carbon\Carbon::now()->subDay(),
		));

		Article::create(array(
			'title'   => 'Third post',
			'slug'    => 'third-post',
			'body'    => 'Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.',
			'user_id' => 1,
		));

		Camera::create(array(
			'title'       => 'Cam1',
			'description' => 'This is cam1',
			'url'         => 'rtmp://localhost/cam1',
			'user_id'     => 1,
		));

        Camera::create(array(
            'title'       => 'Cam2',
            'description' => 'This is cam2',
            'url'         => 'rtmp://localhost/cam2',
            'user_id'     => 1,
        ));

        Camera::create(array(
            'title'       => 'Cam3',
            'description' => 'This is cam3',
            'url'         => 'rtmp://localhost/cam3',
            'user_id'     => 1,
        ));
	}

}
