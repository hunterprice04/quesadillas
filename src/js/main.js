$(document).ready(function(){
    $(".hyperimage").tapestry({
        n_tiles: 4,
        width: 1024,
        height: 1024,
        n_timesteps:500,
        animation_interval: 500, 
    });

    // variableNames = ["Accumulated_Precipitation","Accumulated_Precipitation","Accumulated_Precipitation"];
    variableNames = $(".hyperimage").eq(0).data("tapestry").settings.variable_list;
    var select = document.getElementsByClassName("variable-select").LFBB,
        option,
        i = 0,
        il = variableNames.length;

    for (; i < il; i++) {
        option = document.createElement('option');
        option.value = variableNames[i];
        option.innerHTML = variableNames[i];
        select.appendChild(option);
    }

    $(".variable-select").on("input", function(){
        console.log($(this).val());
        $(".hyperimage").eq(0).data("tapestry")
            .settings.variable=$(this).val();
        $(".hyperimage").eq(0).data("tapestry").render(0);
    });
});
