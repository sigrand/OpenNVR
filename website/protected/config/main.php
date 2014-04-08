<?php
return array(
	'basePath'=>dirname(__FILE__).DIRECTORY_SEPARATOR.'..',
	'name'=>'My Web Application',
	'sourceLanguage' => 'en',
	'behaviors'=>array(
		'onBeginRequest' => array(
			'class' => 'application.components.behaviors.BeginRequest'
			),
		),
	'preload'=>array('log'),
	'import'=>array(
		'application.models.*',
		'application.components.*',
		),

	'modules'=>array(
		'gii'=>array(
			'class'=>'system.gii.GiiModule',
			'password'=>'123',
			'ipFilters'=>array('127.0.0.1','::1'),
			),
		),
	'components'=>array(
		'request'=>array(
			'enableCsrfValidation' => true,
			'enableCookieValidation' => true,
			),
		'user'=>array(
			'allowAutoLogin' => true,
			),
		'urlManager'=>array(
			'urlFormat'=>'path',
			'rules'=>array(
				'/'=>'cams/index',
				'cams/existence/id/<id:\d+>/<stream:.*?>'=>'cams/existence',
				),
			),
		'db'=>array(
			'connectionString' => 'mysql:host=localhost;dbname=cams',
			'emulatePrepare' => true,
			'username' => 'root',
			'password' => '',
			'charset' => 'utf8',
			),
		'errorHandler'=>array(
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
	'params'=>array(
		'languages' => 
		array(
			'en'=>'English',
			'ru'=>'Russian',
			'cn'=>'Chinese'
			),
		),
	);