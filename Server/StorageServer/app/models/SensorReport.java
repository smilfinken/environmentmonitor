package models;

import com.fasterxml.jackson.databind.node.ObjectNode;

import java.time.Instant;

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

    @Constraints.Required
    private Long created;

    public SensorReport(String sensorId, Float temperature, int pressure, int humidity) {
        this.sensorId = sensorId;
        this.temperature = temperature;
        this.pressure = pressure;
        this.humidity = humidity;
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

    public Long getCreated() {
        return created;
    }

    public ObjectNode toJson() {
        ObjectNode result = Json.newObject();
        result.put("sensorId", sensorId);
        result.put("created", created);
        result.put("temperature", temperature);
        result.put("pressure", pressure);
        result.put("humidity", humidity);
        return result;
    }
}
