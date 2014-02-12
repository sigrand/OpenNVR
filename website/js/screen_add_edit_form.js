var cams = {};
var cams_num = 0;
var grid_size = 5; // grid size
$(document).ready(function() {
	$("#screen").css("width", Math.round(parseInt($("#screen").width())/grid_size)*grid_size+2);
	$("#screen").css("height", Math.round((parseInt($("#screen").css("width")) * screen.height / screen.width)/grid_size)*grid_size+2);
	screen_width  = (parseInt($("#screen").css("width"))-2);
	screen_height = (parseInt($("#screen").css("height"))-2);
	x_grid = (screen_width/100)*grid_size;
	y_grid = (screen_height/100)*grid_size;
	$("#add_cam_a").click(function(){
		if (cams_num >= 16) {
			alert("Максимальное количество камер на экране 16!");
			return false;
		}
		cams_num++;
		cam_num = cams_num;
		for (i=0; i<=16; i++) {
			if (($("#Screens_cam"+i+"_h").val() == 0) && ($("#Screens_cam"+i+"_w").val() == 0)) {
				cam_num = i;
				break;
			}
		}
		$("#cam"+cam_num+"_div").show();
		$("#cam"+cam_num+"_div").css("position", "absolute");
		$("#cam"+cam_num+"_div").css("width", (screen_width/100)*10);
		$("#cam"+cam_num+"_div").css("height", (screen_height/100)*10);
		$("#Screens_cam"+cam_num+"_x").val(0);
		$("#Screens_cam"+cam_num+"_y").val(0);
		$("#Screens_cam"+cam_num+"_w").val(Math.round(parseInt(x_grid*2)*100/screen_width));
		$("#Screens_cam"+cam_num+"_h").val(Math.round(parseInt(y_grid*2)*100/screen_height));
	});
	if ($("#new_screen")[0]) {
		// создаем новый экран
		$("#add_cam_a").click();
	} else {
		// изменяем существующий экран, для этого надо отрисовать его камеры
		for (i=1; i<=16; i++) {
			if (($("#Screens_cam"+i+"_h").val() > 0) && ($("#Screens_cam"+i+"_w").val() > 0)) {
				$("#cam"+i+"_div").css("left", Math.round($("#Screens_cam"+i+"_x").val()*screen_width/100));
				$("#cam"+i+"_div").css("top", Math.round($("#Screens_cam"+i+"_y").val()*screen_height/100));
				$("#cam"+i+"_div").css("width", Math.round($("#Screens_cam"+i+"_w").val()*screen_width/100));
				$("#cam"+i+"_div").css("height", Math.round($("#Screens_cam"+i+"_h").val()*screen_height/100));
				$("#cam"+i+"_div").css("position", "absolute");
				$("#cam"+i+"_div").show();
				$("#cam"+i+"_div").resize();
			}
		}
	}
	$(".cams").resizable({minWidth: x_grid*2, minHeight: y_grid*2,
			grid: [ x_grid, y_grid ], resize: function( event, ui ) {
				cam_id = parseInt(ui.element[0].id.substr(3,2));
				div_left = parseInt($("#"+this.id).position().left);
				div_top = parseInt($("#"+this.id).position().top);
				if (div_left + ui.size.width > screen_width) {
					ui.element.css("width", screen_width - div_left);
				}
				if (div_top + ui.size.height > screen_height) {
					ui.element.css("height", screen_height - div_top);
				}
				// сохраняем размеры камер в процентах от размера экрана
				$("#Screens_cam"+cam_id+"_w").val(Math.round(parseInt(ui.size.width)*100/screen_width));
				$("#Screens_cam"+cam_id+"_h").val(Math.round(parseInt(ui.size.height)*100/screen_height));
			}});
	$(".cams").draggable({containment: "#screen", snap:true, scroll: false, cursor: "crosshair",
			grid: [ x_grid, y_grid ], stop: function( event, ui ) {
				cam_id = parseInt(this.id.substr(3,2));

				drag_div_left = $("#"+this.id).position().left;
				drag_div_top = $("#"+this.id).position().top;
				// сохраняем координаты камер в процентах от размера экрана
				$("#Screens_cam"+cam_id+"_x").val(Math.round(parseInt(drag_div_left)*100/screen_width)+"");
				$("#Screens_cam"+cam_id+"_y").val(Math.round(parseInt(drag_div_top)*100/screen_height)+"");
			}});
	$(".cams").hover(function() {
			$(this).css('cursor','move');
		}, function() {
			 $(this).css('cursor','auto');
	});
	$(".remove_cam_button").hover(function() {
			$(this).css('cursor','pointer');
		}, function() {
			 $(this).css('cursor','auto');
	});
	$(".remove_cam_button").click(function() {
		cam_num = parseInt(this.parentElement.id.substr(3,2));
		$("#cam"+cam_num+"_div").hide();
		$("#cam"+cam_num+"_div").css("position", "absolute");
		$("#cam"+cam_num+"_div").css("width", x_grid*2);
		$("#cam"+cam_num+"_div").css("height",y_grid*2);
		$("#Screens_cam"+cam_num+"_x").val(0);
		$("#Screens_cam"+cam_num+"_y").val(0);
		$("#Screens_cam"+cam_num+"_w").val(0);
		$("#Screens_cam"+cam_num+"_h").val(0);
	});
});
