<?php
	echo "<h2>$model->name</h2>";
	// max 16 cams on screen
	$colors = array(
		1 => "#FF0000",
		2 => "#00FF00",
		3 => "#0000FF",
		4 => "#000000",
		5 => "#FFFF00",
		6 => "#FF00FF",
		7 => "#00FFFF",
		8 => "#080000",
		9 => "#000800",
		10 => "#000008",
		11 => "#800000",
		12 => "#008000",
		13 => "#000080",
		14 => "#008080",
		15 => "#800080",
		16 => "#808000",
	);
	for ($i=1; $i <= 16; $i++) {
		eval("\$id=\$model->cam${i}_id;");
		eval("\$x=\$model->cam${i}_x;");
		eval("\$y=\$model->cam${i}_y;");
		eval("\$w=\$model->cam${i}_w;");
		eval("\$h=\$model->cam${i}_h;");
		eval("\$descr=\$model->cam${i}_descr;");
		if (($w > 0) && ($h > 0)) {
			echo '<div class="cams" id="cam'.$i.'_div" style="background-color:'.$colors[$i].';';
			echo ' position:absolute;width:'.$w.'%;height:'.$h.'%;left:'.$x.'%;top:'.$y.'%;">';
			echo "Камера:".$id."<br>";
			echo "Описание:".$descr."<br>";
			echo '</div>';
		}
	}
?>
