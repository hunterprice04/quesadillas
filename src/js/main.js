$(document).ready(function(){
    $(".hyperimage").tapestry({
        n_tiles: 16,
        width: 1024,
        height: 1024,
        n_timesteps:500,
    });

    // Listen to slider events and change the 
    // isosurface threshold accordingly
    // $(".timestep-slider").on("input", function(){
    //     $(".hyperimage").eq(1).data("tapestry")
    //         .current_timestep=[parseInt($(this).val())];
    //     $(".hyperimage").eq(1).data("tapestry").render(0);
    // });
});
