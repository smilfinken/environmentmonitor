package controllers;

import javax.inject.Inject;

import play.mvc.*;
import play.libs.Json;
import play.libs.ws.*;

import views.html.*;

import java.util.concurrent.CompletionStage;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.node.ArrayNode;

public class StatisticsController extends Controller {
    @Inject WSClient ws;

    public Result index() {
        return ok(index.render());
    }

    public CompletionStage<Result> graph() {
        return ws
            .url("http://tiger.smilfinken.net:9000/list")
            .addHeader("Content-Type", "application/json")
            .get()
            .thenApply(result -> {
                ArrayNode labels = Json.newArray();
                ArrayNode temperature = Json.newArray();
                ArrayNode humidity = Json.newArray();
                ArrayNode pressure = Json.newArray();
                JsonNode values = result.getBody(WSBodyReadables.instance.json());
                if (values.isArray()) {
                    Long startValue = values.get(0).findPath("created").longValue();
                    for (JsonNode node : values) {
                        labels.add(node.findPath("created").longValue() - startValue);
                        temperature.add(node.findPath("temperature").floatValue());
                        humidity.add(node.findPath("humidity").intValue());
                        pressure.add(node.findPath("pressure").intValue());
                    }
                }
                return ok(graph.render(labels.toString(), temperature.toString(), humidity.toString(), pressure.toString()));
            });
    }
}
