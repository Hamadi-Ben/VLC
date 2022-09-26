# Groupe de mchind_h 976166
_Choix du sujet : **video filter**_

## Etape 0 : Installation de VLC
Voici les commandes utilisé pour l'Installation de VLC:
```sudo apt-get install git g++ make libtool automake autopoint pkg-config flex bison lua5.2```

```git clone git://git.videolan.org/vlc.git```

```cd vlc```

```./bootstrap```

```./configure```

```make```

Et enfin nous avons lancés VLC avec la commande ```./vlc``` pour vérifier que le fichier fonctionne bien.

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

## Etape 3 : noircir la zone

Pour noircir la zone flouter par le filtre nous avons tout simplement initialiser la valeur des pixels concernés à 0 sur la couche Y.

## Etape 4 : modifier le zoom interactif pour accepter le RGB

Pour modifier le zoom interactif afin que celui ci accepte le RGB, nous avons rajouter ces lignes de codes aux switch case : #"

```C
switch( p_filter->fmt_in.i_codec )
    {
    CASE_PLANAR_RGB
    case VLC_CODEC_RED:
        //printf("yes rgb")
        break;
    default:
        msg_err( p_filter, "Unsupported chroma %4.4s", (char *)&p_filter->fmt_in.i_codec );
        return VLC_EGENERIC;
    }
    if( !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) )
    {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }
```
