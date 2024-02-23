// Mostly started from:
// https://www.freecodecamp.org/news/how-to-build-a-web-app-with-go-gin-and-react-cffdc473576/
// https://hakaselogs.me/2018-04-20/building-a-web-app-with-go-gin-and-react/
// https://github.com/codehakase/golang-gin
// Then repair the bitrot...

package machine_application

import (
	"context"
	_ "encoding/json"
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	_ "time"
  
	"crypto/rsa"
	"crypto/x509"
	"encoding/pem"

        "gopkg.in/yaml.v2"

	// v1 -> stale and insecure dependencies...
	//jwtmiddleware "github.com/auth0/go-jwt-middleware"
	jwtmiddleware "github.com/auth0/go-jwt-middleware/v2"
	"github.com/auth0/go-jwt-middleware/v2/validator" // from v2 examples

	// https://github.com/auth0/go-jwt-middleware/issues/95 (2021-07-07)
	// unmaintained, use golang-jwt instead of form3tech
	// https://github.com/form3tech-oss/jwt-go - archived 2021
	// --> https://github.com/golang-jwt/jwt

	//jwt "github.com/dgrijalva/jwt-go"
	_ "github.com/golang-jwt/jwt"
	// $ go mod tidy
	// $ rm -rf `find ~ ...dgrijalva`
	// $ rm -rf `find ~ ...form3tech`
	// remove from go.mod (form3tech)

	// remove from go.sum (2x form3tech)
	// github.com/auth0/go-jwt-middleware@v1.0.1/jwtmiddleware.go:11:2:
	// github.com/auth0/go-jwt-middleware@v1.0.1 requires
	// github.com/form3tech-oss/jwt-go@v3.2.2+incompatible: missing go.sum entry for go.mod file;
	// to add it: go mod download github.com/form3tech-oss/jwt-go

	"github.com/gin-gonic/contrib/static"
	"github.com/gin-gonic/gin"
)

//-------------------------------------------------------------------//

type Secrets struct {
	Issuer		string `yaml:"issuer"`
	ClientID	string `yaml:"client-id"`
	Audience	[]string `yaml:"audience"`
	Certificate	string `yaml:"certificate"`
	NotThere	int `ymal:"not-there"`
}

func (s Secrets) Auth0Audience() string {
	return s.Audience[0]
}

func (s Secrets) Auth0Domain() string {
	d := strings.TrimPrefix(s.Issuer, "https://")
	d = strings.TrimSuffix(d, "/")
	return d
}

func init() {
	fmt.Println("Init here!")

	data, err := os.ReadFile(os.ExpandEnv("${HOME}/cnc-secrets.yaml"))
	if err != nil {
                log.Fatalf("error: %v", err)
	}

        err = yaml.Unmarshal(data, &cncSecrets)
	if err != nil {
		log.Fatalf("error: %v", err)
	}
        fmt.Printf("--- secrets:\n%#v\n\n", cncSecrets)

}

var (
	cncSecrets = Secrets{}

	// "To call your API you should use the access_token instead of the id_token"

	// https://manage.auth0.com/dashboard/us/dev-<...>/logs -- success only logins.

	// client id?

	// The signing key [secret?] for the token.
	//signingKey = []byte("...")

	// This actually works for RS256
/*
	pubPEM = []byte(`
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----`)
*/
	ErrKeyMustBePEMEncoded = errors.New("Invalid Key: Key must be a PEM encoded PKCS1 or PKCS8 key")
	ErrNotRSAPrivateKey    = errors.New("Key is not a valid RSA private key")
	ErrNotRSAPublicKey     = errors.New("Key is not a valid RSA public key")

	// The issuer [domain?] of our token.
	//issuer = "https://dev-<...>.us.auth0.com/" // XXX full URL required

	// The audience of our token -- who is intended to process the JWT
	//audience = []string{"https://localhost:3000/my-api-endpoint"}

	// Our token must be signed using this data.
	//keyFunc = func(ctx context.Context) (interface{}, error) {
	//	return signingKey, nil
	//}
	keyFunc = func(ctx context.Context) (interface{}, error) {
		key, ok := ParseRSAPublicKeyFromPEM([]byte(cncSecrets.Certificate))
		//log.Printf("RS256 key=%v ok=%v", key, ok)
		return key, ok
	}

	// We want this struct to be filled in with
	// our custom claims from the token.
	customClaims = func() validator.CustomClaims {
		return &CustomClaimsExample{}
	}
)

func ParseRSAPublicKeyFromPEM(key []byte) (*rsa.PublicKey, error) {
	var err error

	// Parse PEM block
	var block *pem.Block
	if block, _ = pem.Decode(key); block == nil {
		return nil, ErrKeyMustBePEMEncoded
	}

	// Parse the key
	var parsedKey interface{}
	if parsedKey, err = x509.ParsePKIXPublicKey(block.Bytes); err != nil {
		if cert, err := x509.ParseCertificate(block.Bytes); err == nil {
			parsedKey = cert.PublicKey
		} else {
			return nil, err
		}
	}

	var pkey *rsa.PublicKey
	var ok bool
	if pkey, ok = parsedKey.(*rsa.PublicKey); !ok {
		return nil, ErrNotRSAPublicKey
	}

	return pkey, nil
}


// checkJWT is a gin.HandlerFunc middleware that checks the validity of the JWT.
func checkJWT() gin.HandlerFunc {

	// Set up the validator.
	jwtValidator, err := validator.New(
		keyFunc,
		//validator.HS256,
		validator.RS256,
		cncSecrets.Issuer,
		cncSecrets.Audience,
		//validator.WithCustomClaims(customClaims),
		//validator.WithAllowedClockSkew(30 * time.Second),
	)
	if err != nil {
		log.Fatalf("Failed to set up the validator: %v", err)
	}

	errorHandler := func(w http.ResponseWriter, r *http.Request, err error) {
		log.Printf("Encountered error while validating JWT: %v", err)
	}

	middleware := jwtmiddleware.New(
		jwtValidator.ValidateToken,
		jwtmiddleware.WithErrorHandler(errorHandler),
	)

	return func(ctx *gin.Context) {
		encounteredError := true

		var handler http.HandlerFunc = func(w http.ResponseWriter, r *http.Request) {
			encounteredError = false
			ctx.Request = r
			ctx.Next()
		}

		middleware.CheckJWT(handler).ServeHTTP(ctx.Writer, ctx.Request)

		if encounteredError {
			ctx.AbortWithStatusJSON(
				http.StatusUnauthorized,
				map[string]string{"message": "Ur JWT is invalid."},
			)
		}
	}
}

//-------------------------------------------------------------------//

type Joke struct {
	ID     int     `json:"id" binding:"required"`
	Likes  int     `json:"likes"`
	Joke   string  `json:"joke" binding:"required"`
}

var jokes = []Joke{
	Joke{1, 0, "Did you hear about the restaurant on the moon? Great food, no atmosphere."},
	Joke{2, 0, "What do you call a fake noodle? An Impasta."},
	Joke{3, 0, "How many apples grow on a tree? All of them."},
	Joke{4, 0, "Want to hear a joke about paper? Nevermind it's tearable."},
	Joke{5, 0, "I just watched a program about beavers. It was the best dam program I've ever seen."},
	Joke{6, 0, "Why did the coffee file a police report? It got mugged."},
	Joke{7, 0, "How does a penguin build it's house? Igloos it together."},
}


// JokeHandler retrieves a list of available jokes
func JokeHandler(c *gin.Context) {
	c.Header("Content-Type", "application/json")
	c.JSON(http.StatusOK, jokes)
}

// LikeJoke increments the likes of a particular joke Item
func LikeJoke(c *gin.Context) {

/*
	bearerToken := c.Request.Header.Get("Authorization")
        reqToken := strings.Split(bearerToken, " ")[1]
	log.Printf("token = %v...", reqToken[:30])

	jwtValidator, err := validator.New(
		keyFunc,
		validator.RS256,
		cncSecrets.Issuer,
		cncSecrets.Audience,
		//validator.WithCustomClaims(customClaims),
		validator.WithAllowedClockSkew(30 * time.Second),
	)
	if err != nil {
		log.Fatalf("failed to set up the validator: %v", err)
	}
	foo, err := jwtValidator.ValidateToken(c, reqToken)
	// could not parse the token: go-jose/go-jose: compact JWS format must have three parts
	// ok=expected claims not validated: go-jose/go-jose/jwt: validation failed, invalid issuer claim (iss)
	// ok=expected claims not validated: go-jose/go-jose/jwt: validation failed, token is expired (exp)
	// validate=&{<nil> {
	//	https://dev-<...>.us.auth0.com/
	//	google-oauth2|nnn...nnn
	//	[ https://localhost:3000/my-api-endpoint
	//	  https://dev-.<...>.us.auth0.com/userinfo ]
	// 1708487796 0 1708480596 }
	// }


	log.Printf("validate=%v ok=%v", foo, err)
	//middleware.CheckJWT(handler).ServeHTTP(ctx.Writer, ctx.Request)

	// ...And process the request anyways...
*/

	jokeid, err := strconv.Atoi(c.Param("jokeID"))
	if err != nil {
		c.AbortWithStatus(http.StatusNotFound)
		return
	}

	for i := 0; i < len(jokes); i++ {
		if jokes[i].ID == jokeid {
			jokes[i].Likes += 1
			log.Printf("+++")
		}
	}

	c.IndentedJSON(http.StatusOK, &jokes)
}

//-------------------------------------------------------------------//

func showIndexPage(c *gin.Context) {
	c.HTML(
		http.StatusOK,
		"index.html",
		gin.H{
			"title": "Home Page",
			"secrets": cncSecrets,
		},
	)
}


func ReactServer() {

	router := gin.Default()

	// Serve frontend static files
	router.Use(
		static.Serve("/js",
		static.LocalFile("www/static/js", true)))

	router.LoadHTMLGlob("www/templates/*")
	router.GET("/", showIndexPage)
	router.GET("/index.html", showIndexPage)

	// Setup route group for the API
	api := router.Group("/api")
	{
		api.GET(
			"/",
			func(c *gin.Context) {
				c.JSON(http.StatusOK, gin.H {
					"message": "pong",
					})
			})

		api.Use(checkJWT())

		// /jokes - retrieve the list of jokes
		api.GET("/jokes", JokeHandler)

		// /jokes/like/:jokeID - record a like
		api.POST("/jokes/like/:jokeID", LikeJoke)
	}

	// Run the server
	go router.Run(":3000")
}

//
