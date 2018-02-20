# --- Created by Ebean DDL
# To stop Ebean DDL generation, remove this comment and start using Evolutions

# --- !Ups

create table sensor_report (
  id                            bigint auto_increment not null,
  sensor_id                     varchar(255),
  temperature                   float,
  pressure                      integer not null,
  humidity                      integer not null,
  time_stamp                    bigint,
  created                       bigint,
  constraint pk_sensor_report primary key (id)
);


# --- !Downs

drop table if exists sensor_report;

