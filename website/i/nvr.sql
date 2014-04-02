-- --------------------------------------------------------
-- Хост:                         127.0.0.1
-- Версия сервера:               5.1.70-community-log - MySQL Community Server (GPL)
-- ОС Сервера:                   Win32
-- HeidiSQL Версия:              8.0.0.4396
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

-- Дамп структуры базы данных cams

-- Дамп структуры для таблица cams.cams
DROP TABLE IF EXISTS `cams`;
CREATE TABLE IF NOT EXISTS `cams` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `server_id` int(10) NOT NULL,
  `user_id` int(10) NOT NULL,
  `name` tinytext NOT NULL,
  `desc` text,
  `url` varchar(2000) NOT NULL,
  `prev_url` varchar(2000) DEFAULT NULL,
  `show` tinyint(1) DEFAULT '0',
  `is_public` tinyint(1) DEFAULT '0',
  `time_offset` varchar(4) NOT NULL DEFAULT '+0',
  `record` tinyint(1) NOT NULL DEFAULT '0',
  `coordinates` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Экспортируемые данные не выделены.


-- Дамп структуры для таблица cams.notifications
DROP TABLE IF EXISTS `notifications`;
CREATE TABLE IF NOT EXISTS `notifications` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `msg` text NOT NULL,
  `creator_id` int(10) NOT NULL DEFAULT '0',
  `dest_id` int(10) NOT NULL DEFAULT '0',
  `shared_id` int(10) NOT NULL DEFAULT '0',
  `status` tinyint(4) NOT NULL DEFAULT '1' COMMENT '1- new, 0 - old, 2 - approved, 3 - disappsoved',
  `time` int(10) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Экспортируемые данные не выделены.


-- Дамп структуры для таблица cams.screens
DROP TABLE IF EXISTS `screens`;
CREATE TABLE IF NOT EXISTS `screens` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `name` varchar(150) DEFAULT NULL,
  `owner_id` int(10) NOT NULL,
  `cam1_id` int(10) NOT NULL,
  `cam1_x` int(10) NOT NULL,
  `cam1_y` int(10) NOT NULL,
  `cam1_w` int(10) NOT NULL,
  `cam1_h` int(10) NOT NULL,
  `cam1_descr` varchar(150) DEFAULT NULL,
  `cam2_id` int(10) NOT NULL,
  `cam2_x` int(10) NOT NULL,
  `cam2_y` int(10) NOT NULL,
  `cam2_w` int(10) NOT NULL,
  `cam2_h` int(10) NOT NULL,
  `cam2_descr` varchar(150) DEFAULT NULL,
  `cam3_id` int(10) NOT NULL,
  `cam3_x` int(10) NOT NULL,
  `cam3_y` int(10) NOT NULL,
  `cam3_w` int(10) NOT NULL,
  `cam3_h` int(10) NOT NULL,
  `cam3_descr` varchar(150) DEFAULT NULL,
  `cam4_id` int(10) NOT NULL,
  `cam4_x` int(10) NOT NULL,
  `cam4_y` int(10) NOT NULL,
  `cam4_w` int(10) NOT NULL,
  `cam4_h` int(10) NOT NULL,
  `cam4_descr` varchar(150) DEFAULT NULL,
  `cam5_id` int(10) NOT NULL,
  `cam5_x` int(10) NOT NULL,
  `cam5_y` int(10) NOT NULL,
  `cam5_w` int(10) NOT NULL,
  `cam5_h` int(10) NOT NULL,
  `cam5_descr` varchar(150) DEFAULT NULL,
  `cam6_id` int(10) NOT NULL,
  `cam6_x` int(10) NOT NULL,
  `cam6_y` int(10) NOT NULL,
  `cam6_w` int(10) NOT NULL,
  `cam6_h` int(10) NOT NULL,
  `cam6_descr` varchar(150) DEFAULT NULL,
  `cam7_id` int(10) NOT NULL,
  `cam7_x` int(10) NOT NULL,
  `cam7_y` int(10) NOT NULL,
  `cam7_w` int(10) NOT NULL,
  `cam7_h` int(10) NOT NULL,
  `cam7_descr` varchar(150) DEFAULT NULL,
  `cam8_id` int(10) NOT NULL,
  `cam8_x` int(10) NOT NULL,
  `cam8_y` int(10) NOT NULL,
  `cam8_w` int(10) NOT NULL,
  `cam8_h` int(10) NOT NULL,
  `cam8_descr` varchar(150) DEFAULT NULL,
  `cam9_id` int(10) NOT NULL,
  `cam9_x` int(10) NOT NULL,
  `cam9_y` int(10) NOT NULL,
  `cam9_w` int(10) NOT NULL,
  `cam9_h` int(10) NOT NULL,
  `cam9_descr` varchar(150) DEFAULT NULL,
  `cam10_id` int(10) NOT NULL,
  `cam10_x` int(10) NOT NULL,
  `cam10_y` int(10) NOT NULL,
  `cam10_w` int(10) NOT NULL,
  `cam10_h` int(10) NOT NULL,
  `cam10_descr` varchar(150) DEFAULT NULL,
  `cam11_id` int(10) NOT NULL,
  `cam11_x` int(10) NOT NULL,
  `cam11_y` int(10) NOT NULL,
  `cam11_w` int(10) NOT NULL,
  `cam11_h` int(10) NOT NULL,
  `cam11_descr` varchar(150) DEFAULT NULL,
  `cam12_id` int(10) NOT NULL,
  `cam12_x` int(10) NOT NULL,
  `cam12_y` int(10) NOT NULL,
  `cam12_w` int(10) NOT NULL,
  `cam12_h` int(10) NOT NULL,
  `cam12_descr` varchar(150) DEFAULT NULL,
  `cam13_id` int(10) NOT NULL,
  `cam13_x` int(10) NOT NULL,
  `cam13_y` int(10) NOT NULL,
  `cam13_w` int(10) NOT NULL,
  `cam13_h` int(10) NOT NULL,
  `cam13_descr` varchar(150) DEFAULT NULL,
  `cam14_id` int(10) NOT NULL,
  `cam14_x` int(10) NOT NULL,
  `cam14_y` int(10) NOT NULL,
  `cam14_w` int(10) NOT NULL,
  `cam14_h` int(10) NOT NULL,
  `cam14_descr` varchar(150) DEFAULT NULL,
  `cam15_id` int(10) NOT NULL,
  `cam15_x` int(10) NOT NULL,
  `cam15_y` int(10) NOT NULL,
  `cam15_w` int(10) NOT NULL,
  `cam15_h` int(10) NOT NULL,
  `cam15_descr` varchar(150) DEFAULT NULL,
  `cam16_id` int(10) NOT NULL,
  `cam16_x` int(10) NOT NULL,
  `cam16_y` int(10) NOT NULL,
  `cam16_w` int(10) NOT NULL,
  `cam16_h` int(10) NOT NULL,
  `cam16_descr` varchar(150) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Экспортируемые данные не выделены.


-- Дамп структуры для таблица cams.servers
DROP TABLE IF EXISTS `servers`;
CREATE TABLE IF NOT EXISTS `servers` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ip` tinytext NOT NULL,
  `protocol` set('http','https') NOT NULL DEFAULT 'http',
  `s_port` int(5) unsigned NOT NULL DEFAULT '8082' COMMENT 'server port',
  `w_port` int(5) unsigned NOT NULL DEFAULT '8084' COMMENT 'web port',
  `l_port` int(5) unsigned NOT NULL DEFAULT '1935' COMMENT 'live port',
  `comment` varchar(500) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Экспортируемые данные не выделены.


-- Дамп структуры для таблица cams.shared
DROP TABLE IF EXISTS `shared`;
CREATE TABLE IF NOT EXISTS `shared` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `owner_id` int(10) NOT NULL,
  `user_id` int(10) NOT NULL,
  `cam_id` int(10) NOT NULL,
  `show` tinyint(4) NOT NULL DEFAULT '0',
  `is_public` tinyint(4) NOT NULL DEFAULT '0',
  `is_approved` tinyint(4) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Дамп структуры для таблица cams.sessions
DROP TABLE IF EXISTS `sessions`;
CREATE TABLE IF NOT EXISTS `sessions` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `user_id` int(10) NOT NULL,
  `session_id` varchar(32) NOT NULL,
  `real_id` varchar(15) NOT NULL,
  `ip` varchar(15) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Дамп структуры для таблица cams.users
DROP TABLE IF EXISTS `users`;
CREATE TABLE IF NOT EXISTS `users` (
  `id` int(7) NOT NULL AUTO_INCREMENT,
  `nick` varchar(150) DEFAULT NULL,
  `email` varchar(250) DEFAULT NULL,
  `pass` varchar(512) DEFAULT NULL,
  `salt` varchar(32) DEFAULT NULL,
  `time_offset` varchar(4) DEFAULT '+0',
  `status` tinyint(4) DEFAULT NULL,
  `options` tinyint(4) DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT=' ';

-- Дамп данных таблицы cams.users: 1 rows
/*!40000 ALTER TABLE `users` DISABLE KEYS */;
INSERT INTO `users` (`id`, `nick`, `email`, `pass`, `salt`, `time_offset`, `status`, `options`) VALUES
  (1, 'admin', 'admin@admin.admin', '$1$Do/.Q40.$ll5Pe7IgHBPLQYU.rzVco0', 'fb3b88dde2e758ecaab215fa25bf3077', '+0', 3, 0);
/*!40000 ALTER TABLE `users` ENABLE KEYS */;
/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
