<?php

// uncomment the following to define a path alias
// Yii::setPathOfAlias('local','path/to/local-folder');
///qqqqqqqqq
// This is the main Web application configuration. Any writable
// CWebApplication properties can be configured here.
return array(
	'basePath'=>dirname(__FILE__).DIRECTORY_SEPARATOR.'..',
	'name'=>'My Web Application',
	'sourceLanguage' => 'ru',
	'behaviors'=>array(
		'onBeginRequest' => array(
			'class' => 'application.components.behaviors.BeginRequest'
			),
		),
	// preloading 'log' component
	'preload'=>array('log'),

	// autoloading model and component classes
	'import'=>array(
		'application.models.*',
		'application.components.*',
		),

	'modules'=>array(
		// uncomment the following to enable the Gii tool
		//*
		'gii'=>array(
			'class'=>'system.gii.GiiModule',
			'password'=>'123',
			// If removed, Gii defaults to localhost only. Edit carefully to taste.
			//'ipFilters'=>array('127.0.0.1','::1'),
			),
		//*/
		),

	// application components
	'components'=>array(
		'request'=>array(
			'enableCsrfValidation'=>true,
			'enableCookieValidation'=>true,
			),
		'user'=>array(
			// enable cookie-based authentication
			'allowAutoLogin'=>true,
			),
		// uncomment the following to enable URLs in path-format
		//*
		'urlManager'=>array(
			'urlFormat'=>'path',
			'rules'=>array(
				'/'=>'cams/index',
				'cams/existence/<stream:\d+|\d+_low>'=>'cams/existence',
				),
			),
		//*/
		'db'=>array(
			'connectionString' => 'mysql:host=localhost;dbname=nvr',
			'emulatePrepare' => true,
			'username' => 'nvr',
			'password' => 'nvr',
			'charset' => 'utf8',
			),
		//*/
		'errorHandler'=>array(
			// use 'site/error' action to display errors
			'errorAction'=>'site/error',
			),
		'log'=>array(
			'class'=>'CLogRouter',
			'routes'=>array(
				array(
					'class'=>'CFileLogRoute',
					'levels'=>'error, warning',
					),
				// uncomment the following to show log messages on web pages
				/*
				array(
					'class'=>'CWebLogRoute',
				),
				//*/
			),
			),
		),

	// application-level parameters that can be accessed
	// using Yii::app()->params['paramName']
	'params'=>array(
	'languages' => 
	array(
		'ru'=>'Russian',
		'en'=>'English',
		'cn'=>'Chinese'
		),
		// this is used in contact page
		'moment_server_protocol' => 'http',
		'moment_server_ip' => 'localhost',
		'moment_server_port' => '8082',
		'moment_web_port' => '8080',
		'moment_live_port' => '1935',
		'adminEmail'=>'webmaster@example.com',
		),
	);