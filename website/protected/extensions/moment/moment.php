<?php

/*

conf_file - это id камеры, тут надо бы убедиться в том, что юниксовая система поймет имя файла такое как id
Живое видео от сервера:

rtmp://<server_ip>:1935/<cam_id>
Записанное видео от сервера

rtmp://127.0.0.1:1935/nvr/sony?start=1368828341
 where start — unixtime of begin playing position from 00:00 1 Jan 1970 in UTC
 sony - name of stream
         //http://127.0.0.1:8082/mod_nvr_admin/rec_on?stream=sony&seq=1 - turn on record
        //http://127.0.0.1:8082/mod_nvr_admin/rec_off?stream=sony&seq=1 - turn off record
*/

 class moment {
    private $http;
    private $options = array(
        'protocol' => 'http',
        'server_ip' => '127.0.0.1',
        'server_port' => '8082',
        );

    public function __construct($http, $options) {
        $this->http = $http;
        $this->http->setPort($this->options['server_port']);
        $this->options = array_merge($this->options, $options);
    }

    public function add($id, $name, $url) {
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/admin/add_channel?conf_file=$id&title=$name&uri=$url");
        return trim($result) == 'OK' ? true : $result;
    }

    public function modify($id, $name, $url) {
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/admin/add_channel?conf_file=$id&title=$name&uri=$url&update");
        return trim($result) == 'OK' ? true : $result;
    }

    public function delete($id) {
        Notify::note("{$this->options['protocol']}://{$this->options['server_ip']}/admin/remove_channel?conf_file=$id");
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/admin/remove_channel?conf_file=$id");         
        return trim($result) == 'OK' ? true : $result;
    }

    public function playlist() {
        $this->http->setPort($this->options['web_port']);
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/server/playlist.json");
        $this->http->setPort($this->options['server_port']);
        return trim($result);
    }

    public function unixtime() {
        $this->http->setPort($this->options['web_port']);
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/mod_nvr/unixtime");
        $this->http->setPort($this->options['server_port']);
        return trim($result);
    }

    public function existence($stream) {
        $this->http->setPort($this->options['web_port']);
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/mod_nvr/existence?stream={$stream}");
        $this->http->setPort($this->options['server_port']);
        return trim($result);
    }

    public function stat() {
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/mod_nvr_admin/statistics");
        return json_decode(trim($result), 1);
    }

    public function rec($mode = 'on', $id) {
        $mode = $mode == 'on' ? 'on' : 'off';
        $result = $this->http->get("{$this->options['protocol']}://{$this->options['server_ip']}/mod_nvr_admin/rec_{$mode}?stream={$id}&seq=1");
        $result = json_decode($result, 1);
        return (bool)trim($result['recording']) == (bool)($mode == 'on' ? true : false);
    }

}

?>