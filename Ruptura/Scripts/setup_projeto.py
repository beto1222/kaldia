# Rode no Unreal Editor: Tools > Execute Python Script... > este arquivo.
# Cria o material PBR base, importa os sons de AudioFonte/ e cria o mapa L_CidadeMorta.
import os
import unreal

PROJ = unreal.Paths.project_dir()
AUDIO_SRC = os.path.join(PROJ, "AudioFonte")
eal = unreal.EditorAssetLibrary


def criar_material():
    path = "/Game/Materials/M_Ruptura_Base"
    if eal.does_asset_exist(path):
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_Ruptura_Base", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    cor = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    cor.set_editor_property("parameter_name", "Cor")
    cor.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1))
    rug = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 200)
    rug.set_editor_property("parameter_name", "Rugosidade")
    rug.set_editor_property("default_value", 0.6)
    met = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 300)
    met.set_editor_property("parameter_name", "Metalico")
    emi = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 400)
    emi.set_editor_property("parameter_name", "Emissivo")
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 400)
    mel.connect_material_expressions(cor, "", mul, "A")
    mel.connect_material_expressions(emi, "", mul, "B")
    mel.connect_material_property(cor, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(mat)
    eal.save_asset(path)
    unreal.log("Ruptura: material criado")


def importar_audio():
    if not os.path.isdir(AUDIO_SRC):
        unreal.log_warning("Ruptura: pasta AudioFonte não encontrada, pulando áudio")
        return
    tasks = []
    for f in sorted(os.listdir(AUDIO_SRC)):
        if f.lower().endswith(".wav"):
            t = unreal.AssetImportTask()
            t.filename = os.path.join(AUDIO_SRC, f)
            t.destination_path = "/Game/Audio"
            t.automated = True
            t.replace_existing = True
            t.save = True
            tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for f in os.listdir(AUDIO_SRC):
        if f.endswith("_loop.wav"):
            p = "/Game/Audio/" + f[:-4]
            a = eal.load_asset(p)
            if a:
                a.set_editor_property("looping", True)
                eal.save_asset(p)
    unreal.log("Ruptura: %d sons importados" % len(tasks))


def criar_mapa():
    path = "/Game/Maps/L_CidadeMorta"
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if eal.does_asset_exist(path):
        les.load_level(path)
    else:
        les.new_level(path)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not any(isinstance(a, unreal.NavMeshBoundsVolume) for a in eas.get_all_level_actors()):
        nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(8000, 0, -200))
        nav.set_actor_scale3d(unreal.Vector(95, 35, 10))
    les.save_current_level()
    unreal.log("Ruptura: mapa pronto. Confira se o NavMeshBoundsVolume aparece verde com a tecla P.")


criar_material()
importar_audio()
criar_mapa()
