<?php

class momentManager {
	public function __construct() {
		$http = new http;
		//$http->setProxy('localhost', 666);
		$http->setTimeout(60);
		$this->moment = new moment(
			$http,
			array(
				'server_ip' => Yii::app()->params['moment_server_ip'],
				'server_port' => Yii::app()->params['moment_server_port'],
				'web_port' => Yii::app()->params['moment_web_port'],
				)
			);
	}

	public function add($model) {
		$result = $this->moment->add($model->id, $model->name, $model->url);
        if($result !== true) {
            Notify::note('Добавление камеры неудалось, проблема с моментом, http ответ: '.$result);
            return false;
        } else {
            if(!empty($model->prev_url)) {
                $this->moment->add($model->id.'_low', $model->name.'_low', $model->prev_url); 
            }
            $this->rec($model->record == 1 ? 'on' : 'off', $model->id);
        }
        return $result;
    }

    public function edit($model) {
        $result = $this->moment->modify($model->id, $model->name, $model->url);
        if($result !== true) {
            Notify::note('Изменение камеры неудалось, проблема с моментом, http ответ: '.$result);
            return false;
        } else {
            if(!empty($model->prev_url)) {
               $result = $this->moment->modify($model->id.'_low', $model->name.'_low', $model->prev_url);
           }
           $this->rec($model->record == 1 ? 'on' : 'off', $model->id);
       }
       return $result;
   }

   public function delete($model) {
    $result = $this->moment->delete($model->id);
    if($result !== true) {
        Notify::note('Удаление камеры неудалось, проблема с моментом, http ответ: '.$result);
        return false;
    } else {
        if(!empty($model->prev_url)) {
            $this->moment->delete($model->id.'_low'); 
        }
    }
    return $result;
}

public function playlist() {
    $result = $this->moment->playlist();
    if(!$result) {
        Notify::note('Неудалось получить плейлист, проблема с моментом');
    }
    return $result;
}

public function existence($stream) {
    $result = $this->moment->existence($stream);
    if(!$result) {
        Notify::note('Неудалось получить архив, проблема с моментом');
    }
    return $result;
}

public function stat($type) {
    $result = $this->moment->stat($type);
    if(!$result) {
        Notify::note('Неудалось получить статистику вида: '.$type.', проблема с моментом');
    }
    return $result;
}

public function rec($mode, $id) {
    $result = $this->moment->rec($mode, $id);
    if(!$result) {
        Notify::note('Неудалось поставить или снять запись, проблема с моментом');
    }
    return $result;
}

public function unixtime() {
    $result = $this->moment->unixtime();
    if(!$result) {
        Notify::note('Неудалось получить unixtime, проблема с моментом');
    }
    return $result;
}
}

?>