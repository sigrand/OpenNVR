<?php
/**
 * Controller is the customized base controller class.
 * All controller classes for this application should extend from this base class.
 */
class Controller extends CController
{
	public $layout='//layouts/column1';
	public $menu=array();
	public $breadcrumbs=array();

	protected function renderJSON($data) {
		$this->layout = false;
		header('Content-type: application/json');
		echo CJSON::encode($data);
		foreach (Yii::app()->log->routes as $route) {
			if($route instanceof CWebLogRoute) {
				$route->enabled = false;
			}
		}
		Yii::app()->end();
	}

	public function render($view, $data = NULL, $return = false) {
		$controller = $this->getId();
		$path = 'application.views.'.$controller.'.'.str_replace('/', '.', $view);
		$path = Yii::getPathOfAlias($path);
		$style = Settings::model()->getValue('style');
		if(file_exists($path.'-'.$style.'.php')) {
			$view = $view.'-'.$style;
		} elseif(file_exists($path.'-default.php')) {
			$view = $view.'-default';
		}
		parent::render($view, $data, $return);
	}

}