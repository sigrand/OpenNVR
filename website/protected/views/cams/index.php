<?php
/* @var $this CamsController */

if(!Yii::app()->user->isGuest) {
	?>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Мои камеры</h3>
			</div>
			<div class="panel-body">
				<ul class="list-group">
					<li class="list-group-item">
						<div class="row">
							<?php if(!empty($myCams)) {
								foreach ($myCams as $key => $cam) {
									?>
									<div class="col-sm-6">
										<object type="application/x-shockwave-flash" data="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" class="img-thumbnail" id="cam_<?php echo $key;?>" style="width:100%;display:none;">
											<param name="bgcolor" value="#ffffff" />
											<param name="allowFullScreen" value="true" />
											<param name="allowScriptAccess" value="always" />
											<param name="wmode" value="window" />
											<param name="movie" value="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" />
											<param name="flashvars" value="aspect=1.33&st=<?php echo Yii::app()->request->baseUrl.'/player/style.txt'; ?>&file=<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>">
										</object><br/>
													<?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
													<?php echo $cam->desc; ?><br/>
									</div>
									<?php
									if($key % 2) {
										?>
									</div>
								</li>
								<li class="list-group-item">
									<div class="row">
										<?php
									}
								}
								?>
								<script type="text/javascript">
								var cam = $("#cam_0");
								var h = cam.innerWidth()/1.3;
								$('object[id*="cam_"]').height(h).show();
								cam.height(h).show();
								</script>
								<?php
							} else {
								?>
								Камер для отображения нет.
								<?php } ?>
							</ul>
						</div>
					</div>
				</div>
				<div class="col-sm-12">
					<div class="panel panel-default">
						<div class="panel-heading">
							<h3 class="panel-title">Расшаренные для меня камеры</h3>
						</div>
						<div class="panel-body">
							<ul class="list-group">
								<li class="list-group-item">
									<div class="row">
										<?php if(!empty($mySharedCams)) {
											foreach($mySharedCams as $key => $cam) {
												if(isset($cam->cam_id)) {
													$cam = $cam->cam;
												}
												?>
												<div class="col-sm-6">
													<object type="application/x-shockwave-flash" data="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" class="img-thumbnail" id="scam_<?php echo $key;?>" style="width:100%;display:none;">
														<param name="bgcolor" value="#ffffff" />
														<param name="allowFullScreen" value="true" />
														<param name="allowScriptAccess" value="always" />
														<param name="wmode" value="window" />
														<param name="movie" value="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" />
														<param name="flashvars" value="aspect=1.33&st=<?php echo Yii::app()->request->baseUrl.'/player/style.txt'; ?>&file=<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>">
													</object><br/>
													<?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
													<?php echo $cam->desc; ?><br/>
												</div>
												<?php
												if($key % 2) {
													?>
												</div>
											</li>
											<li class="list-group-item">
												<div class="row">
													<?php
												}
											}
											?>
											<script type="text/javascript">
											var cam = $("#scam_0");
											var h = cam.innerWidth()/1.3;
											$('object[id*="scam_"]').height(h).show();
											cam.height(h).show();
											</script>
										</div>
									</li>
									<?php
								} else {
									?>
									Камер для отображения нет.
									<?php } ?>
								</ul>
							</div>
						</div>
					</div>
					<?php
				}
				?>
				<div class="col-sm-12">
					<div class="panel panel-default">
						<div class="panel-heading">
							<h3 class="panel-title">Публичные камеры</h3>
						</div>
						<div class="panel-body">
							<ul class="list-group">
								<li class="list-group-item">
									<div class="row">
										<?php if(!empty($myPublicCams)) {
											foreach ($myPublicCams as $key => $cam) {
												if(isset($cam->cam_id)) {
													$cam = $cam->cam;
												}
												?>
												<div class="col-sm-6">
													<object type="application/x-shockwave-flash" data="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" class="img-thumbnail" id="pcam_<?php echo $key;?>" style="width:100%;display:none;">
														<param name="bgcolor" value="#ffffff" />
														<param name="allowFullScreen" value="true" />
														<param name="allowScriptAccess" value="always" />
														<param name="wmode" value="window" />
														<param name="movie" value="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" />
														<param name="flashvars" value="aspect=1.33&st=<?php echo Yii::app()->request->baseUrl.'/player/style.txt'; ?>&file=<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>">
													</object><br/>
													<?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
													<?php echo $cam->desc; ?><br/>
												</div>
												<?php
												if($key % 2) {
													?>
												</div>
											</li>
											<li class="list-group-item">
												<div class="row">
													<?php
												}
											}
											?>
											<script type="text/javascript">
											var cam = $("#pcam_0");
											var h = cam.innerWidth()/1.3;
											$('object[id*="pcam_"]').height(h).show();
											cam.height(h).show();
											</script>
										</div>
									</li>
									<?php
								} else {
									?>
									Камер для отображения нет.
									<?php } ?>
								</ul>
							</div>
						</div>
					</div>