<?php
/* @var $this CamsController */
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
				<?php if (Yii::app()->user->permissions == 3) { ?>
				<h3 class="panel-title">Мои экраны</h3>
				<?php } else { ?>
				<h3 class="panel-title">Все экраны</h3>
				<?php } ?>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link('<img src="'.Yii::app()->request->baseUrl.'/images/add-icon.png" style="width:32px;"> Добавить экран', $this->createUrl('screens/create'));
				if(!empty($myscreens)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th>#</th>
							<?php if (Yii::app()->user->permissions == 3) { ?>
							<th>ID владельца</th>
							<?php } ?>
							<th>Название</th>
							<th>Редактировать</th>
							<th>Удалить</th>
						</thead>
						<tbody>
							<?php
/*							echo CHtml::beginForm($this->createUrl('screens/manage'), "post");*/
							foreach ($myscreens as $key => $screen) {
								echo '<tr>
								<td>'.($key+1).'</td>';
								if (Yii::app()->user->permissions == 3)
									echo '<td>'.$screen->owner_id.'</td>';
								echo '<td>'.CHtml::link(CHtml::encode($screen->name), $this->createUrl('screens/view/', array('id' => $screen->id)), array('target' => '_blank')).'</td>
								<td>'.CHtml::link('Редактировать', $this->createUrl('screens/update', array('id' => $screen->id))).'</td>
								<td>'.CHtml::link('Удалить', $this->createUrl('screens/delete', array('id' => $screen->id))).'</td>
								</tr>';
							}
							?>
						</tbody>
					</table>
					<?php
				} else {
					?>
					<br/>
					Список моих экранов пуст.
					<br/>
					<?php
				}
				echo $addLink;
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