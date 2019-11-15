CREATE TABLE IF NOT EXISTS `wind` (
  `date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `direction` int(11) NOT NULL,
  `speed` float NOT NULL,
  KEY `date` (`date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
