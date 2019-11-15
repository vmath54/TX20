CREATE TABLE IF NOT EXISTS `badframes` (
  `date` datetime NOT NULL,
  `duration` int(11) NOT NULL,
  `records` int(11) NOT NULL,
  `frames` int(11) NOT NULL,
  `badframes` int(11) NOT NULL,
  KEY `date` (`date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
