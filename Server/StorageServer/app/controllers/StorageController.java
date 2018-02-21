package controllers;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;

import java.util.List;

import play.libs.Json;
import play.mvc.*;

import models.*;

public class StorageController extends Controller {
    public Result list() {
        ArrayNode result = Json.newArray();
        List<SensorReport> sensorReports = SensorReport.find.all();
        for (SensorReport item : sensorReports) {
            result.add(item.toJson());
        }
        return ok(result);
    }

    public Result report() {
        JsonNode json = request().body().asJson();

        String sensorId = json.findPath("sensorId").textValue();
        if(sensorId == null) {
            return badRequest("missing parameter [sensorId]");
        }

        Float temperature = json.findPath("temperature").floatValue();
        if(temperature == null) {
            return badRequest("missing parameter [temperature]");
        }

        Integer pressure = json.findPath("pressure").intValue();
        if(pressure == null) {
            return badRequest("missing parameter [pressure]");
        }

        Integer humidity = json.findPath("humidity").intValue();
        if(humidity == null) {
            return badRequest("missing parameter [humidity]");
        }

        SensorReport sensorReport = new SensorReport(sensorId, temperature, pressure, humidity);
        sensorReport.save();

        ObjectNode result = Json.newObject();
        result.put("status", true);
        return ok(result);
    }
}
