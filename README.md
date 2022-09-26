# Groupe de mchind_h 976166
_Choix du sujet : **video filter**_

## Etape 0.1 : modifier VLC
Nous avons changer l'aspect graphique, noux avons modifier le `"VLC media player"` dans le fichier `main_interface.cpp` :

```cpp
void MainInterface::setVLCWindowsTitle( const QString& aTitle )
{
    if( aTitle.isEmpty() )
    {
        setWindowTitle( qtr( "APE ETNA" ) );
    }
    else
    {
        setWindowTitle( aTitle + " - " + qtr( "APE ETNA" ) );
    }
}
```
## Etape 1 : utiliser le filtre de gaussianblur

Pour utiliser le filtre gaussianblur nous avons lancés la commande : 
`./vlc --video-filter gaussianblur --gaussainblur-sigma=10.0 https://www.youtube.com/watch?v=9T7g0-Tcvg0`