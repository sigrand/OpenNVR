<?php
/* @var $this AdminController */
?>
<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Мои камеры</h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($myCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAll", array ("class" => "checkAll")); ?></th>
							<th>#</th>
							<th>Название</th>
							<th>Описание</th>
							<th>Статус</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/cams'), "post");
							foreach ($myCams as $key => $cam) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'cam_'.$cam->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($cam->name).'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->public ? 'Публичная' : 'Не публичная').'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="4"></td>
								<td><?php echo CHtml::submitButton('Опубликовать/Скрыть', array('name' => 'public', 'class' => 'btn btn-success')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					?>
					Список моих камер пуст.
					<?php
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Публичные камеры</h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($publicCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllSh", array ("class" => "checkAllSh")); ?></th>
							<th>#</th>
							<th>Название</th>
							<th>Описание</th>
							<th>Статус</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/cams'), "post");
							foreach ($publicCams as $key => $cam) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'shcam_'.$cam->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($cam->name).'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->public ? 'Публичная' : 'Не публичная').'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="4"></td>
								<td><?php echo CHtml::submitButton('Опубликовать/Скрыть', array('name' => 'public', 'class' => 'btn btn-success')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					?>
					Список публичных камер пуст.
					<br/>
					<?php
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Все остальные камеры</h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($allCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllP", array ("class" => "checkAllP")); ?></th>
							<th>#</th>
							<th>Название</th>
							<th>Описание</th>
							<th>Статус</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/cams'), "post");
							foreach ($allCams as $key => $cam) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'pcam_'.$cam->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($cam->name).'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->public ? 'Публичная' : 'Не публичная').'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="3">
									<?php
									$this->widget('CLinkPager', array(
										'pages' => $pages,
										'header' => 'Перейти на страницу: ',
										'nextPageLabel' => 'далее',
										'prevPageLabel' => 'назад',
										));
										?>
										</td>
										<td></td>
										<td><?php echo CHtml::submitButton('Опубликовать/Скрыть', array('name' => 'public', 'class' => 'btn btn-success')); ?></td>
									</tr>
									<?php echo CHtml::endForm(); ?>
								</tbody>
							</table>
							<?php
						} else {
							?>
							Список камер пуст.
							<br/>
							<?php
						}
						?>
					</div>
				</div>
			</div>
			<script>
			$(document).ready(function(){
				$(".checkAll").click(function(){
					$('input[id*="CamsForm_cam_"]').not(this).prop('checked', this.checked);
				});
				$(".checkAllSh").click(function(){
					$('input[id*="CamsForm_shcam_"]').not(this).prop('checked', this.checked);
				});
				$(".checkAllP").click(function(){
					$('input[id*="CamsForm_pcam_"]').not(this).prop('checked', this.checked);
				});
			});
			</script>