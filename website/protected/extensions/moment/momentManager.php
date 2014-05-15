<?php

class momentManager {
	public function __construct($id) {
        $server = Servers::model()->findByPK($id);
        $http = new http;
        $http->setTimeout(10);
        $this->moment = new moment(
            $http,
            array(
                'server_ip' => $server->ip,
                'server_port' => $server->s_port,
                'web_port' => $server->w_port,
                )
            );
    }

    public function add($model) {
        if(!is_null($model->user) && !empty($model->user)) {
            $u = parse_url($model->url);
            $usr = $model->user;
            $ps = $model->pass;
            $url = $u['scheme'].'://'.
            (!is_null($usr) && !empty($usr) ? $usr.(!is_null($ps) && !empty($ps) ? ':'.$ps : '').'@' : '').
            $u['host'].
            (isset($u['port']) ? ':'.$u['port'] : '').
            $u['path'].
            (isset($u['query']) ? '?'.$u['query'] : '');
        } else {
            $url = $model->url;
        }
        $result = $this->moment->add($model->id, $model->name, $url);
        if($result !== true) {
            Notify::note(Yii::t('errors', 'Add cam fail, problem with nvr, http response: {response}', array('{response}' => $result)));
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
            Notify::note(Yii::t('errors', 'Change cam fail, problem with nvr, http response: {response}', array('{response}' => $result)));
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
            Notify::note(Yii::t('errors', 'Delete cam fail, problem with nvr, http response: {response}', array('{response}' => $result)));
            return false;
        } else {
            if(!empty($model->prev_url)) {
                $this->moment->delete($model->id.'_low'); 
            }
        }
        return $result;
    }

    public function playlist($cam_id, $session_id) {
        $result = $this->moment->playlist();
        if(!$result) {
            Notify::note(Yii::t('errors', 'Can not get playlist, problem with nvr'));
        }
        $result = json_decode($result, 1);
        $response = array('sources' => array());
        if(!isset($result['sources'])) { return json_encode($response); }
        foreach ($result['sources'] as $value) {
            if($value['name'] == $cam_id) {
                $value['name'] = $session_id;
                $response['sources'][] = $value;
                break;
            }
        }
        return json_encode($response);
    }

    public function existence($stream) {
        $result = $this->moment->existence($stream);
        if(!$result) {
            Notify::note(Yii::t('errors', 'Can not get archive, problem with nvr'));
        }
        return $result;
    }

    public function stat($type) {
        $result = $this->moment->stat($type);
        if(!$result) {
            Notify::note(Yii::t('errors', 'Can not get statistics type: {type}', array('{type}' => $type)));
        }
        return $result;
    }

    public function rec($mode, $id) {
        $result = $this->moment->rec($mode, $id);
        if(!$result) {
            Notify::note(Yii::t('errors', 'Record on/off fail, problem with nvr'));
        }
        return $result;
    }

    public function unixtime() {
        $result = $this->moment->unixtime();
        if(!$result) {
            Notify::note(Yii::t('errors', 'Unixtime get fail, problem with nvr'));
        }
        return $result;
    }
}

?>