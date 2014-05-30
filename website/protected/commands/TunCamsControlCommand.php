<?php
class TunCamsControlCommand extends CConsoleCommand {
	private function printUsage() {
		echo "Usage: ./yiic TunCamsControl add|remove IP_ADDR MAC\n";
	}
	public function run($args) {
		if (sizeOf($args) < 2) {
			$this->printUsage();
		} else {
			$ip = $args[1];
			if ($args[0] == "add") {
				if (sizeOf($args) < 3) $this->printUsage();
				$mac = $args[2];
				if (!$model) $model = TunCam::model()->findByAttributes(array('mac' => $mac));
				if (!$model) $model = TunCam::model()->findByAttributes(array('ip' => $ip));
				if ($model) {
					if ($model->cams_id == "0") {
						echo "Error cam with this mac or ip already added but not activated!\n";
						exit(1);
					} else {
						$cam_model = Cams::model()->findByPK($model->cams_id);
						if (!$cam_model) {
							echo "Error cam with this mac or ip already added and activated but cam model not found!\n";
							exit(1);
						}
						Yii::import('ext.moment.index', 1);
						$momentManager = new momentManager($cam_model->server_id);
						if ($momentManager) {
							if ($momentManager->add($cam_model)) {
								echo "OK\n";
								exit(0);
							} else {
								echo "Error momentManager->add cams_id=$model->cams_id\n";
								exit(1);
							}
						} else {
							echo "Error can't create momentManager with server_id=$cam_model->server_id\n";
							exit(1);
						}
					}
				} else {
					$model = new TunCam;
					$model->mac = $mac;
					$model->ip = $ip;
					$model->cams_id = NULL;
					$model->add_time = date('Y-m-d H:i:s');
					if (!$model->validate()) {
						echo "Error validate model!!!\n";
						exit(1);
					} else {
						if (!$model->save()) {
							echo "Error save model!!!\n";
							exit(1);
						}
						echo "OK\n";
						exit(0);
					}
				}
			} else {
				if ($args[0] == "remove") {
					$model = TunCam::model()->findByAttributes(array('ip' => $ip));
					if ($model) {
						if ($model->cams_id == "0") {
							echo "Warning cam with IP=$ip not activated yet!!!\n";
							exit(0);
						}
						$cam_model = Cams::model()->findByPK($model->cams_id);
						if (!$cam_model) {
							echo "Warning cams_model with real_id=$model->cams_id not found\n";
							exit(0);
						}
						Yii::import('ext.moment.index', 1);
						$momentManager = new momentManager($cam_model->server_id);
						if ($momentManager) {
							if (!$momentManager->delete($cam_model)) {
								echo "Error momentManager->delete cams_id=$model->cams_id\n";
								exit(1);
							} else {
								echo "OK\n";
								exit(0);
							}
						} else {
							echo "Error can't create momentManager with server_id=$cam_model->server_id\n";
							exit(1);
						}
					} else {
						echo "Error find model with IP=$ip\n";
						exit(1);
					}
				} else {
					$this->printUsage();
				}
			}
		}
	}
}
?>