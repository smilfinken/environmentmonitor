package models;

import com.fasterxml.jackson.databind.node.ObjectNode;

import java.time.Instant;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;

import io.ebean.Finder;
import io.ebean.Model;
import play.data.validation.Constraints;
import play.libs.Json;

@Entity
public class SensorReport extends Model {
    @Id
    private Long id;

    @Constraints.Required
    private String sensorId;

    @Constraints.Required
    private Float temperature;

    @Constraints.Required
    private int pressure;

    @Constraints.Required
    private int humidity;

    @Column
    private Float particles;

    @Constraints.Required
    private Long created;

    public SensorReport(String sensorId, Float temperature, int pressure, int humidity) {
        this.sensorId = sensorId;
        this.temperature = temperature;
        this.pressure = pressure;
        this.humidity = humidity;
        this.particles = 0f;
        this.created = Instant.now().toEpochMilli();
    }

    public SensorReport(String sensorId, Float temperature, int pressure, int humidity, Float particles) {
        this.sensorId = sensorId;
        this.temperature = temperature;
        this.pressure = pressure;
        this.humidity = humidity;
        this.particles = particles;
        this.created = Instant.now().toEpochMilli();
    }

    public static final Finder<Long, SensorReport> find = new Finder<>(SensorReport.class);

    public String getSensorId() {
        return sensorId;
    }

    public Float getTemperature() {
        return temperature;
    }

    public int getPressure() {
        return pressure;
    }

    public int getHumidity() {
        return humidity;
    }

    public Float getParticles() {
        return particles;
    }

    public Long getCreated() {
        return created;
    }

    public ObjectNode toJson() {
        ObjectNode result = Json.newObject();
        result.put("sensorId", getSensorId());
        result.put("created", getCreated());
        result.put("temperature", getTemperature());
        result.put("pressure", getPressure());
        result.put("humidity", getHumidity());
        result.put("particles", getParticles());
        return result;
    }
}
