<link href="<?php echo Yii::app()->request->baseUrl; ?>/player/css/dark-hive/jquery-ui-1.10.4.custom.css" rel="stylesheet">
<script src="<?php echo Yii::app()->request->baseUrl; ?>/player/js/jquery-ui-1.10.4.custom.js"></script>
<div class="col-sm-offset-2 col-sm-8">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<?php echo Yii::t('cams', 'Quotas manage'); ?>
			</div>
			<div class="panel-body">
				<div class="panel panel-default">
					<div class="panel-body">
						<div class="col-sm-12"><?php echo Yii::t('cams', 'Current quota'); ?> :
							<div class="progress">
								<?php
								foreach ($myCams as $key => $cam) {
									$name = CHtml::encode($cam->name);
									?>
									<div class="progress-bar cams progress-bar-<?php echo $key%2==0 ? 'success' : 'primary'; ?>" style="display:none;" id="pb_<?php echo $cam->id;?>" title="<?php echo $name; ?>">
										<?php echo $name; ?>
									</div>
									<?php
								}
								?>
								<div class="progress-bar progress-bar-info" style="width: 100%" id="available" title="100% available">
									100% available
								</div>
							</div>
						</div>
					</div>
				</div>
				<?php
				if(!empty($myCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAll", array ("class" => "checkAll")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('cams', 'Name'); ?></th>
							<th><?php echo Yii::t('cams', 'Record?'); ?></th>
							<th><?php echo Yii::t('cams', 'Quota'); ?></th>
							<tbody>
								<?php
								echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
								foreach ($myCams as $key => $cam) {
									$name = CHtml::link(CHtml::encode($cam->name), $this->createUrl('cams/archive', array('id' => $cam->getSessionId(), 'full' => '1')), array('target' => '_blank'));
									echo '<tr>
									<td>'.CHtml::activeCheckBox($form, 'cam_'.$cam->getSessionId()).'</td>
									<td>'.($key+1).'</td>
									<td>'.$name.'</td>
									<td>'.($cam->record ? Yii::t('cams', 'Record') : Yii::t('cams', 'Don\'t record')).'</td>
									<td><div id="q_'.$cam->id.'">0%</div><br/><div class="slider-range" id="'.$cam->id.'"></div></td>
									</tr>';
								}
								?>
								<?php echo CHtml::endForm(); ?>
							</tbody>
						</table>
						<?php
					} else {
						echo Yii::t('cams', '<br/>My cams list is empty.<br/>');
					}
					?>
				</div>
			</div>
		</div>
		<script>
		var max = 100;
		var available = 100;
		var sum = {};

		function sumAll() {
			var all = 0;
			for(key in sum) {
				all += sum[key];
			}
			return all;
		}

		function recount() {
			$('.cams').each(function(i,v){
				sum[$(v).attr('id')] = parseInt(getPercent($(v)));
			});
		}
		var getPercent = function(elem){
			var elemName = elem.attr("id");
			var width = elem.width();
			var parentWidth = elem.offsetParent().width();
			var percent = Math.round(100*width/parentWidth);
			return percent;
		}

		$(function() {
			recount();
			$('.slider-range').slider({
				range: false,
				min: 0,
				max: 100,
				step: 1,
				values: [ 0 ],
				start: function(event, ui) {
					var value = getPercent($('#pb_' + $(this).attr('id')));
					var max = (100-sumAll());
					$(this).slider('values', 0, value);
					$(this).slider('option', 'max', (value + max));
				},
				slide: function(event, ui) {
					var id = $(this).attr('id');
					var value = 0;
					var progressBar = $('#pb_' + id);
					value = parseInt(ui.values[0]);
					$('#q_' + id).text(value + '%');
					progressBar.show().width(value + '%').text(value + '% ' + progressBar.attr('title'));
					sum['pb_' + id] = value;
					$('#available').width((100-sumAll()) + '%').text((100-sumAll()) + '% ' + 'available');
				}
			});
		});
		</script>